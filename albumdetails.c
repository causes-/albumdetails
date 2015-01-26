#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <tag_c.h>
#include <sys/stat.h>

#include "arg.h"
#include "util.h"

#define LEN(x) (sizeof (x) / sizeof *(x))

char *argv0;
bool utf8flag = true;
bool siunits = false;

struct albumdetails {
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
};

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
};

struct intcount {
	int number;
	int count;
};

struct strcount {
	char *str;
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

int readfiles(struct tracks **t, char **files, int nfiles) {
	int i, j;
	TagLib_File *file;
	TagLib_Tag *tag;
	const TagLib_AudioProperties *properties;

	for (i = 0, j = 0; i < nfiles; i++) {
		file = taglib_file_new(files[i]);
		if (!file) {
			fprintf(stderr, "non-audio file: %s\n", files[i]);
			continue;
		}

		tag = taglib_file_tag(file);
		properties = taglib_file_audioproperties(file);
		if (!tag || !properties) {
			fprintf(stderr, "Can't read meta-data for %s\n", files[i]);
		} else {
			strlcpy((*t)[j].artist, taglib_tag_artist(tag), sizeof (*t)[j].artist);
			strlcpy((*t)[j].album, taglib_tag_album(tag), sizeof (*t)[j].album);
			strlcpy((*t)[j].genre, taglib_tag_genre(tag), sizeof (*t)[j].genre);
			(*t)[j].year = taglib_tag_year(tag);
			(*t)[j].bitrate = taglib_audioproperties_bitrate(properties);
			(*t)[j].samplerate = taglib_audioproperties_samplerate(properties);
			(*t)[j].channels = taglib_audioproperties_channels(properties);

			(*t)[j].track = taglib_tag_track(tag);
			strlcpy((*t)[j].title, taglib_tag_title(tag), sizeof (*t)[j].title);
			(*t)[j].length = taglib_audioproperties_length(properties);
			(*t)[j].size = filesize(files[i]);

			j++;
		}

		taglib_tag_free_strings();
		taglib_file_free(file);

		*t = erealloc(*t, (j + 2) * sizeof(struct tracks));
	}

	if (!j)
		eprintf("Couldn't find any audio files\n");
	return j;
}

void strcount(struct strcount **strc, char *token) {
	int i;

	for (i = 0; (*strc)[i].str; i++)
		if (!strcmp((*strc)[i].str, token)) {
			(*strc)[i].count++;
			return;
		}

	(*strc)[i].str = token;
	(*strc)[i].count++;
	if (i)
		*strc = erealloc(*strc, (i + 2) * sizeof(struct strcount));
	(*strc)[i+1].str = NULL;
	(*strc)[i+1].count = 0;
}

char *strmostcommon(struct strcount *str, bool artist) {
	int i;
	int max = 0;
	char *p = NULL;

	for (i = 0; str[i].str; i++)
		if (str[i].count > max) {
			max = str[i].count;
			p = str[i].str;
		}

	if (artist && i > 3)
		p = "VA";

	return p;
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

void getaverages(struct albumdetails *ad, struct tracks *t) {
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

	ad->avgbitrate = 0;
	ad->length = 0;
	ad->size = 0;

	for (i = 0; i < ad->tracks; i++) {
		ad->length += t[i].length;
		ad->size += t[i].size;
		ad->avgbitrate += t[i].bitrate;

		strcount(&artists, t[i].artist);
		strcount(&albums, t[i].album);
		strcount(&genres, t[i].genre);
		intcount(&years, t[i].year);
		intcount(&samplerates, t[i].samplerate);
		intcount(&channels, t[i].channels);
	}

	ad->avgbitrate /= ad->tracks;
	ad->artist = strmostcommon(artists, true);
	ad->album = strmostcommon(albums, false);
	ad->genre = strmostcommon(genres, false);
	ad->year = intmostcommon(years);
	ad->samplerate = intmostcommon(samplerates);
	ad->channels = intmostcommon(channels);

	free(artists);
	free(albums);
	free(genres);
	free(years);
	free(samplerates);
	free(channels);
}

void printdetails(struct albumdetails *ad, struct tracks *t) {
	int i;

	printf("Artist: %s\n", ad->artist);
	printf("Album: %s\n", ad->album);
	printf("Genre: %s\n", ad->genre);
	printf("Year: %d\n", ad->year);
	printf("Quality: %dkbps / %.1fkHz / %d channels\n\n",
			ad->avgbitrate, ad->samplerate / 1000.0f, ad->channels);

	for (i = 0; i < ad->tracks; i++)
		if (!strncmp(ad->artist, "VA", 2))
			printf("%2d. %s - %s (%s)\n",
					t[i].track, t[i].artist, t[i].title, secondstostr(t[i].length));
		else
			printf("%2d. %s (%s)\n",
					t[i].track, t[i].title, secondstostr(t[i].length));

	printf("\n");
	printf("Playing time: %s\n", secondstostr(ad->length));
	printf("Total size: %s\n", bytestostr(ad->size));
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
	struct albumdetails ad;
	struct tracks *t;

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

	t = emalloc(sizeof(struct tracks));

	taglib_set_strings_unicode(utf8flag);

	ad.tracks = readfiles(&t, argv, argc);

	getaverages(&ad, t);

	printdetails(&ad, t);

	return EXIT_SUCCESS;
}
