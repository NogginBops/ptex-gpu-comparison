
#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <tinydir.h>

#include <assert.h>

#include <stdint.h>

void print_usage()
{
	printf("usage: ImageComparer.exe path1 path2\n");
}

float calcSquareDiff(char v1, char v2)
{
	float f1 = v1 / 255.0f;
	float f2 = v2 / 255.0f;

	float diff = f1 - f2;

	return diff * diff;
}

int main(int argc, char* argv[])
{
	if (argc != 3) {
		// TODO: Maybe read in files?
		printf("Must be called with two file paths.\n");
		print_usage();
		return -1;
	}

	tinydir_file file1;
	tinydir_file file2;

	if (tinydir_file_open(&file1, argv[1]) == -1)
	{
		printf("Could not open file '%s'\n", argv[1]);
		return -1;
	}

	if (tinydir_file_open(&file2, argv[2]) == -1)
	{
		printf("Could not open file '%s'\n", argv[2]);
		return -1;
	}

	struct {
		char* filename;
		char* data;
		int width, height, channels;
	} image1, image2;

	image1.filename = argv[1];
	image2.filename = argv[2];

	image1.data = stbi_load(image1.filename, &image1.width, &image1.height, &image1.channels, 4);

	if (image1.data == NULL) {
		printf("Could not open image '%s'.\n", image1.filename);
		return -1;
	}

	image2.data = stbi_load(image2.filename, &image2.width, &image2.height, &image2.channels, 4);

	if (image2.data == NULL) {
		printf("Could not open image '%s'.\n", image2.filename);
		return -1;
	}

	printf("img1: '%s', width: %d, height: %d, comps: %d\n", image1.filename, image1.width, image1.height, image1.channels);
	printf("img2: '%s', width: %d, height: %d, comps: %d\n", image2.filename, image2.width, image2.height, image2.channels);

	assert(image1.width == image2.width && "The two images have differing width!");
	assert(image1.height == image2.height && "The two images have differing height!");

	float redSME = 0, greenSME = 0, blueSME = 0;

	int diffs = 0;

	char* img1 = image1.data;
	char* img2 = image2.data;
	for (int y = 0; y < image1.height; y++) {
		for (int x = 0; x < image1.width; x++) {
			int index = (y * image1.height + x) * 4;
			float red_diff = calcSquareDiff(img1[index + 0], img2[index + 0]);

			redSME   += red_diff;
			greenSME += calcSquareDiff(img1[index+1], img2[index+1]);
			blueSME  += calcSquareDiff(img1[index+2], img2[index+2]);

			if (red_diff != 0) {
				printf("red diff at x: %d, y: %d, ref: %u, comp: %u\n", x, y, (uint8_t)img1[index], (uint8_t)img2[index]);
				diffs++;
			}
		}
	}

	redSME /= (image1.width * image1.height);
	greenSME /= (image1.width * image1.height);
	blueSME /= (image1.width * image1.height);

	printf("redSME: %g, greenSME: %g, blueSME: %g\n", redSME, greenSME, blueSME);
	printf("%d different red pixels\n", diffs);

	stbi_image_free(image1.data);
	stbi_image_free(image2.data);
	return 0;
}


