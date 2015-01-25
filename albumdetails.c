#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <tag_c.h>
#include <sys/stat.h>

#include "arg.h"
#include "util.h"

#define LEN(x) (sizeof (x) / sizeof *(x))

char *argv0;
bool utf8flag = true;
bool siunits = false;

struct tracklist {
	int tracks;
	char *artist;
	char *album;
	char *genre;
	int year;
	int avgbitrate;
	int samplerate;
	int channels;
	int length;
	int size;
	struct tracks {
		char artist[BUFSIZ];
		char album[BUFSIZ];
		char genre[BUFSIZ];
		int year;
		int bitrate;
		int samplerate;
		int channels;
		int track;
		char title[BUFSIZ];
		int length;
		int size;
	} *t;
};

struct intcount {
	int number;
	int count;
};

struct strcount {
	char str[BUFSIZ];
	int count;
};

int filesize(char *filename) {
	struct stat st;

	stat(filename, &st);
	return st.st_size;
}

char *secondstostr(int seconds) {
	static char buf[32];
	int h, m, s;

	h = seconds / 3600;
	m = (seconds / 60) % 60;
	s = seconds % 60;

	if (h)
		snprintf(buf, sizeof buf, "%02d:%02d:%02d", h, m, s);
	else
		snprintf(buf, sizeof buf, "%02d:%02d", m, s);

	return buf;
}

char *bytestostr(double bytes) {
	int i;
	int cols;
	static char buf[32];
	static const char iec[][4] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB" };
	static const char si[][3] = { "B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB" };
	const char *unit;
	char *fmt;
	double prefix;

	if (siunits) {
		prefix = 1000.0;
		cols = LEN(si);
	} else {
		prefix = 1024.0;
		cols = LEN(iec);
	}

	for (i = 0; bytes >= prefix && i < cols; i++)
		bytes /= prefix;

	fmt = i ? "%.2f %s" : "%.0f %s";
	unit = siunits ? si[i] : iec[i];
	snprintf(buf, sizeof buf, fmt, bytes, unit);

	return buf;
}

void readfiles(struct tracklist *tl, char **filenames, int n) {
	int i, j;
	TagLib_File *file;
	TagLib_Tag *tag;
	const TagLib_AudioProperties *properties;

	for (i = 0, j = 0; i < n; i++) {
		file = taglib_file_new(filenames[i]);
		if (!file) {
			fprintf(stderr, "non-audio file: %s\n", filenames[i]);
			continue;
		}

		tag = taglib_file_tag(file);
		properties = taglib_file_audioproperties(file);
		if (!tag || !properties) {
			fprintf(stderr, "Can't read meta-data for %s\n", filenames[i]);
		} else {
			strlcpy(tl->t[j].artist, taglib_tag_artist(tag), sizeof tl->t[j].artist);
			strlcpy(tl->t[j].album, taglib_tag_album(tag), sizeof tl->t[j].album);
			strlcpy(tl->t[j].genre, taglib_tag_genre(tag), sizeof tl->t[j].genre);
			tl->t[j].year = taglib_tag_year(tag);
			tl->t[j].bitrate = taglib_audioproperties_bitrate(properties);
			tl->t[j].samplerate = taglib_audioproperties_samplerate(properties);
			tl->t[j].channels = taglib_audioproperties_channels(properties);

			tl->t[j].track = taglib_tag_track(tag);
			strlcpy(tl->t[j].title, taglib_tag_title(tag), sizeof tl->t[j].title);
			tl->t[j].length = taglib_audioproperties_length(properties);
			tl->t[j].size = filesize(filenames[i]);

			j++;
		}

		taglib_tag_free_strings();
		taglib_file_free(file);

		tl->t = erealloc(tl->t, (j + 2) * sizeof(struct tracks));
	}

	if (!j)
		eprintf("Couldn't find any audio files\n");
	tl->tracks = j;
}

void intcount(struct intcount **intc, int number) {
	int i;

	for (i = 0; (*intc)[i].number; i++)
		if ((*intc)[i].number == number) {
			(*intc)[i].count++;
			return;
		}

	(*intc)[i].number = number;
	(*intc)[i].count++;
	if (i)
		*intc = erealloc(*intc, (i + 2) * sizeof(struct intcount));
	(*intc)[i+1].number = 0;
	(*intc)[i+1].count = 0;
}

int intmostcommon(struct intcount *intc) {
	int i;
	int max = 0;
	int retval = 0;

	for (i = 0; intc[i].number; i++)
		if (intc[i].count > max) {
			max = intc[i].count;
			retval = intc[i].number;
		}

	return retval;
}

void strcount(struct strcount **strc, char *token) {
	int i;

	for (i = 0; (*strc)[i].str[0]; i++)
		if (!strncmp((*strc)[i].str, token, sizeof (*strc)[i].str)) {
			(*strc)[i].count++;
			return;
		}

	strlcpy((*strc)[i].str, token, sizeof (*strc)[i].str);
	(*strc)[i].count++;
	if (i)
		*strc = erealloc(*strc, (i + 2) * sizeof(struct strcount));
	(*strc)[i+1].str[0] = '\0';
	(*strc)[i+1].count = 0;
}

char *strmostcommon(struct strcount *str, bool artist) {
	int i;
	int max = 0;
	char *p = NULL;

	for (i = 0; str[i].str[0]; i++)
		if (str[i].count > max) {
			max = str[i].count;
			p = str[i].str;
		}

	if (artist && i > 3)
		p = "VA";

	return p;
}

void getaverages(struct tracklist *tl) {
	int i;
	struct strcount *artists;
	struct strcount *albums;
	struct strcount *genres;
	struct intcount *years;
	struct intcount *samplerates;
	struct intcount *channels;

	artists = ecalloc(2, sizeof(struct strcount));
	albums = ecalloc(2, sizeof(struct strcount));
	genres = ecalloc(2, sizeof(struct strcount));
	years = ecalloc(2, sizeof(struct intcount));
	samplerates = ecalloc(2, sizeof(struct intcount));
	channels = ecalloc(2, sizeof(struct intcount));

	tl->avgbitrate = 0;
	tl->length = 0;
	tl->size = 0;

	for (i = 0; i < tl->tracks; i++) {
		tl->length += tl->t[i].length;
		tl->size += tl->t[i].size;
		tl->avgbitrate += tl->t[i].bitrate;

		strcount(&artists, tl->t[i].artist);
		strcount(&albums, tl->t[i].album);
		strcount(&genres, tl->t[i].genre);
		intcount(&years, tl->t[i].year);
		intcount(&samplerates, tl->t[i].samplerate);
		intcount(&channels, tl->t[i].channels);
	}

	tl->avgbitrate /= tl->tracks;
	tl->artist = strmostcommon(artists, true);
	tl->album = strmostcommon(albums, false);
	tl->genre = strmostcommon(genres, false);
	tl->year = intmostcommon(years);
	tl->samplerate = intmostcommon(samplerates);
	tl->channels = intmostcommon(channels);
}

void printdetails(struct tracklist *tl) {
	int i;

	printf("Artist: %s\n", tl->artist);
	printf("Album: %s\n", tl->album);
	printf("Genre: %s\n", tl->genre);
	printf("Year: %d\n", tl->year);
	printf("Quality: %dkbps / %.1fkHz / %d channels\n\n",
			tl->avgbitrate, tl->samplerate / 1000.0f, tl->channels);

	for (i = 0; i < tl->tracks; i++)
		if (!strncmp(tl->artist, "VA", 3))
			printf("%2d. %s - %s (%s)\n",
					tl->t[i].track, tl->t[i].artist, tl->t[i].title, secondstostr(tl->t[i].length));
		else
			printf("%2d. %s (%s)\n",
					tl->t[i].track, tl->t[i].title, secondstostr(tl->t[i].length));

	printf("\n");
	printf("Playing time: %s\n", secondstostr(tl->length));
	printf("Total size: %s\n", bytestostr(tl->size));
}

void usage(void) {
	eprintf("Usage: %s [options] <audiofiles>\n"
			"\n"
			"-h    Help\n"
			"-i    Use ISO-8859-1 instead of UTF-8\n"
			"-s    Use SI units instead of IEC\n"
			, argv0);
}

int main(int argc, char **argv) {
	struct tracklist tl;

	ARGBEGIN {
	case 'i':
		utf8flag = false;
		break;
	case 's':
		siunits = true;
		break;
	default:
		usage();
	} ARGEND;

	if (argc < 1)
		usage();

	taglib_set_strings_unicode(utf8flag);

	tl.t = emalloc(sizeof(struct tracks));

	readfiles(&tl, argv, argc);

	getaverages(&tl);

	printdetails(&tl);

	return EXIT_SUCCESS;
}
