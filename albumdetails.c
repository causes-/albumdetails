#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <tag_c.h>
#include <sys/stat.h>

#include "arg.h"

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

void eprintf(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void *emalloc(size_t size) {
	void *p;

	p = malloc(size);
	if (!p)
		eprintf("Out of memory\n");
	return p;
}

void *erealloc(void *p, size_t size) {
	void *r;

	r = realloc(p, size);
	if (!r)
		eprintf("Out of memory\n");
	return r;
}

size_t strlcpy(char *dest, const char *src, size_t size) {
	size_t len;

	len = strlen(src);

	if (size) {
		if (len >= size)
			size -= 1;
		else
			size = len;
		strncpy(dest, src, size);
		dest[size] = '\0';
	}

	return size;
}

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
		snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
	else
		snprintf(buf, sizeof(buf), "%02d:%02d", m, s);

	return buf;
}

char *bytestostr(double bytes) {
	int i;
	int cols;
	static char str[32];
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
	snprintf(str, sizeof(str), fmt, bytes, unit);

	return str;
}

struct tracklist *readfiles(struct tracklist *tl, char **argv, int argc) {
	int i;
	TagLib_File *file;
	TagLib_Tag *tag;
	const TagLib_AudioProperties *properties;

	tl->tracks = 0;

	for (i = 0; i < argc; i++) {
		file = taglib_file_new(argv[i]);
		if (!file) {
			fprintf(stderr, "non-audio file: %s\n", argv[i]);
			continue;
		}

		tag = taglib_file_tag(file);
		properties = taglib_file_audioproperties(file);
		if (!tag || !properties) {
			fprintf(stderr, "Can't read meta-data for %s\n", argv[i]);
		} else {
			strlcpy(tl->t[tl->tracks].artist, taglib_tag_artist(tag), sizeof tl->t[tl->tracks].artist);
			strlcpy(tl->t[tl->tracks].album, taglib_tag_album(tag), sizeof tl->t[tl->tracks].album);
			strlcpy(tl->t[tl->tracks].genre, taglib_tag_genre(tag), sizeof tl->t[tl->tracks].genre);
			tl->t[tl->tracks].year = taglib_tag_year(tag);
			tl->t[tl->tracks].bitrate = taglib_audioproperties_bitrate(properties);
			tl->t[tl->tracks].samplerate = taglib_audioproperties_samplerate(properties);
			tl->t[tl->tracks].channels = taglib_audioproperties_channels(properties);

			tl->t[tl->tracks].track = taglib_tag_track(tag);
			strlcpy(tl->t[tl->tracks].title, taglib_tag_title(tag), sizeof tl->t[tl->tracks].title);
			tl->t[tl->tracks].length = taglib_audioproperties_length(properties);
			tl->t[tl->tracks].size = filesize(argv[i]);

			tl->tracks++;
		}

		taglib_tag_free_strings();
		taglib_file_free(file);

		tl->t = erealloc(tl->t, (tl->tracks + 2) * sizeof(struct tracks));
	}

	if (!tl->tracks)
		eprintf("Couldn't find any audio files\n");
	return tl;
}

void intcount(struct intcount *intc, int number) {
	int j;

	for (j = 0; intc[j].number ; j++) {
		if (intc[j].number == number) {
			intc[j].count++;
			return;
		}
	}

	intc[j+1].number = 0;
	intc[j].number = number;
	intc[j].count++;
}

int intmostcommon(struct intcount *intc) {
	int j;
	int max = 0;
	int retval;

	for (j = 0; intc[j].number; j++)
		if (intc[j].count > max) {
			max = intc[j].count;
			retval = intc[j].number;
		}

	return retval;
}

void strcount(struct strcount *strc, char *token) {
	int j;

	for (j = 0; strc[j].str[0]; j++)
		if (!strncmp(strc[j].str, token, sizeof strc[j].str)) {
			strc[j].count++;
			return;
		}

	strc[j+1].str[0] = '\0';
	strlcpy(strc[j].str, token, sizeof strc[j].str);
	strc[j].count++;
}

char *strmostcommon(struct strcount *str, bool artist) {
	int j;
	int max = 0;
	char *p;

	for (j = 0; str[j].str[0]; j++)
		if (str[j].count > max) {
			max = str[j].count;
			p = str[j].str;
		}

	if (j > 3 && artist)
		p = "VA";

	return p;
}

struct tracklist *getaverages(struct tracklist *tl) {
	int i;
	struct strcount artists[64];
	struct strcount albums[64];
	struct strcount genres[64];
	struct intcount years[64];
	struct intcount samplerates[64];
	struct intcount channels[64];

	tl->avgbitrate = 0;
	tl->length = 0;
	tl->size = 0;

	for (i = 0; i < tl->tracks; i++) {
		tl->length += tl->t[i].length;
		tl->size += tl->t[i].size;
		tl->avgbitrate += tl->t[i].bitrate;

		strcount(artists, tl->t[i].artist);
		strcount(albums, tl->t[i].album);
		strcount(genres, tl->t[i].genre);
		intcount(years, tl->t[i].year);
		intcount(samplerates, tl->t[i].samplerate);
		intcount(channels, tl->t[i].channels);
	}

	tl->avgbitrate /= tl->tracks;
	tl->artist = strmostcommon(artists, true);
	tl->album = strmostcommon(albums, false);
	tl->genre = strmostcommon(genres, false);
	tl->year = intmostcommon(years);
	tl->samplerate = intmostcommon(samplerates);
	tl->channels = intmostcommon(channels);

	return tl;
}

void printdetails(struct tracklist *tl) {
	int i;

	printf("Artist ....: %s\n", tl->artist);
	printf("Album .....: %s\n", tl->album);
	printf("Genre .....: %s\n", tl->genre);
	printf("Year ......: %d\n", tl->year);
	printf("Quality ...: %dkbps / %.1fkHz / %d channels\n\n",
			tl->avgbitrate, tl->samplerate / 1000.0f, tl->channels);

	for (i = 0; i < tl->tracks; i++)
		if (!strncmp(tl->artist, "VA", 3))
			printf("%2d. %s - %s (%s)\n",
					tl->t[i].track, tl->t[i].artist, tl->t[i].title, secondstostr(tl->t[i].length));
		else
			printf("%2d. %s (%s)\n",
					tl->t[i].track, tl->t[i].title, secondstostr(tl->t[i].length));

	printf("\n");
	printf("Playing time ...: %s\n", secondstostr(tl->length));
	printf("Total size .....: %s\n", bytestostr(tl->size));
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
	struct tracklist *tl;

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

	tl = emalloc(sizeof(struct tracklist));
	tl->t = emalloc(sizeof(struct tracks));

	tl = readfiles(tl, argv, argc);

	tl = getaverages(tl);

	printdetails(tl);

	return EXIT_SUCCESS;
}
