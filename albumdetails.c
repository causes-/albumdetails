#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <tag_c.h>

char *argv0;

void eprintf(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
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

void usage(void) {
	eprintf("usage: %s *.mp3\n", argv0);
}

int main(int argc, char **argv) {
	int i;
	TagLib_File *file;
	TagLib_Tag *tag;
	const TagLib_AudioProperties *properties;

	struct albumdetails {
		char artist[BUFSIZ];
		char album[BUFSIZ];
		char genre[BUFSIZ];
		int year;

		struct quality {
			int bitrate;
			float samplerate;
			int channels;
		} q;

		struct tracklist {
			int track;
			char title[BUFSIZ];
			int min;
			int sec;
		} tl[64];
	} ad;

	memset(&ad, 0, sizeof(ad));

	argv0 = argv[0];

	if (argc < 2)
		usage();

	taglib_set_strings_unicode(true);

	for (i = 1; i < argc; i++) {
		file = taglib_file_new(argv[i]);
		if (!file)
			break;

		tag = taglib_file_tag(file);
		properties = taglib_file_audioproperties(file);

		if (tag && properties) {
			strlcpy(ad.artist, taglib_tag_artist(tag), sizeof ad.artist);
			strlcpy(ad.album, taglib_tag_album(tag), sizeof ad.album);
			strlcpy(ad.genre, taglib_tag_genre(tag), sizeof ad.genre);
			ad.year = taglib_tag_year(tag);
			ad.q.bitrate = taglib_audioproperties_bitrate(properties);
			ad.q.samplerate = taglib_audioproperties_samplerate(properties) / 1000.0;
			ad.q.channels = taglib_audioproperties_channels(properties);

			ad.tl[i-1].track = taglib_tag_track(tag);
			strlcpy(ad.tl[i-1].title, taglib_tag_title(tag), sizeof ad.tl[i-1].title);
			ad.tl[i-1].sec = taglib_audioproperties_length(properties) % 60;
			ad.tl[i-1].min = (taglib_audioproperties_length(properties) - ad.tl[i-1].sec) / 60;
		}

		taglib_tag_free_strings();
		taglib_file_free(file);
	}

	printf("Artist ....: %s\n", ad.artist);
	printf("Album .....: %s\n", ad.album);
	printf("Genre .....: %s\n", ad.genre);
	printf("Year ......: %d\n", ad.year);
	printf("Quality ...: %dkbps / %.1fkHz / %d channels\n\n",
			ad.q.bitrate, ad.q.samplerate, ad.q.channels);

	for (i = 0; ad.tl[i].title[0]; i++) {
		printf("%2d. %-56s\t(%d:%02d)\n",
				ad.tl[i].track, ad.tl[i].title, ad.tl[i].min, ad.tl[i].sec);
	}

	return EXIT_SUCCESS;
}
