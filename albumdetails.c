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

struct track {
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

int readfiles(struct track **t, char **files, int nfiles) {
	int i, j;
	TagLib_File *file;
	TagLib_Tag *tag;
	const TagLib_AudioProperties *properties;

	for (i = 0, j = 0; i < nfiles; i++) {
		file = taglib_file_new(files[i]);
		if (!file) {
			fprintf(stderr, "Non-audio file: %s\n", files[i]);
			continue;
		}

		tag = taglib_file_tag(file);
		properties = taglib_file_audioproperties(file);
		if (!tag || !properties) {
			fprintf(stderr, "Can't read meta-data for: %s\n", files[i]);
			continue;
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
			*t = erealloc(*t, (j + 1) * sizeof(struct track));
		}

		taglib_tag_free_strings();
		taglib_file_free(file);
	}

	if (!j)
		eprintf("Couldn't find any audio files\n");
	return j;
}

char *strfreq(char *p, int len, bool artist) {
	int i, j;
	int max;
	char *ret;
	struct strcount {
		char *str;
		int count;
	} *strc;

	strc = ecalloc(2, sizeof(struct strcount));

	// calculate occurence count for each str
	for (i = 0; i < len; i++) {
		for (j = 0; strc[j].str; j++) {
			if (!strcmp(strc[j].str, p)) {
				strc[j].count++;
				break;
			}
		}

		strc[j].str = p;
		strc[j].count = 1;
		strc[j+1].str = NULL;
		if (j)
			strc = erealloc(strc, (j + 2) * sizeof(struct strcount));
		p += sizeof(struct track) / sizeof(char);
	}

	// find most common str
	for (i = 0, max = 0; strc[i].str; i++) {
		if (strc[i].count > max) {
			max = strc[i].count;
			ret = strc[i].str;
		}
	}
	if (artist && i > 3)
		ret = "VA";

	free(strc);
	return ret;
}

int intfreq(int *p, int len) {
	int i, j;
	int ret, max;
	struct intcount {
		int number;
		int count;
	} *intc;

	intc = ecalloc(2, sizeof(struct intcount));

	// calculate occurence count for each int
	for (i = 0; i < len; i++) {
		for (j = 0; intc[j].number; j++) {
			if (intc[j].number == *p) {
				intc[j].count++;
				break;
			}
		}

		intc[j].number = *p;
		intc[j].count = 1;
		intc[j+1].number = 0;
		if (j)
			intc = erealloc(intc, (j + 2) * sizeof(struct intcount));
		p += sizeof(struct track) / sizeof(int);
	}

	// find most common int
	for (i = 0, max = 0; intc[i].number; i++) {
		if (intc[i].count > max) {
			max = intc[i].count;
			ret = intc[i].number;
		}
	}

	free(intc);
	return ret;
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
	int i;
	int tracks;
	int length = 0;
	int size = 0;
	int avgbitrate = 0;
	struct track *t;
	char *artist;

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

	t = emalloc(sizeof(struct track));

	taglib_set_strings_unicode(utf8flag);

	tracks = readfiles(&t, argv, argc);

	for (i = 0; i < tracks; i++) {
		length += t[i].length;
		size += t[i].size;
		avgbitrate += t[i].bitrate;
	}
	avgbitrate /= tracks;

	artist = strfreq(t[0].artist, tracks, true);
	printf("Artist: %s\n", artist);
	printf("Album: %s\n", strfreq(t[0].album, tracks, false));
	printf("Genre: %s\n", strfreq(t[0].genre, tracks, false));
	printf("Year: %d\n", intfreq(&t[0].year, tracks));
	printf("Quality: %dkbps / %.1fkHz / %d channels\n\n",
			avgbitrate,
			intfreq(&t[0].samplerate, tracks) / 1000.0f,
			intfreq(&t[0].channels, tracks));

	for (i = 0; i < tracks; i++) {
		if (!strcmp(artist, "VA")) {
			printf("%2d. %s - %s (%s)\n",
					t[i].track, t[i].artist, t[i].title, secondstostr(t[i].length));
		} else {
			printf("%2d. %s (%s)\n",
					t[i].track, t[i].title, secondstostr(t[i].length));
		}
	}

	printf("\n");
	printf("Playing time: %s\n", secondstostr(length));
	printf("Total size: %s\n", bytestostr(size));

	return EXIT_SUCCESS;
}
