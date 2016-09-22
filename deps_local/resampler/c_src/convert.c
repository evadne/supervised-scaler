#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vips/vips.h>

void process (char *lineBuffer);

int main (int argc, char **argv) {
  //
  // Start libVIPS at the beginning of the program so once it starts accepting input
  // it is actually ready.
  //
  if (VIPS_INIT(argv[0])) {
    vips_error_exit(NULL);
  }

  //
  // Spin up an infinite loop to accept conversion requests from STDIN
  // with the format: <width> <height> <file path>.
  //
  // Note: fgets blocks, without burning CPU
  //
  char lineBuffer[4096];
  while (fgets(lineBuffer, 4096, stdin)) {
    //
    // Start the main process loop again.
    //
    process(lineBuffer);
  }
  
  return 0;
}

void process (char *lineBuffer) {
  //
  // Attempt to scan information from input string.
  //
  char fromFilePath[4096];
  int toWidth = 0, toHeight = 0;
  if (3 != sscanf(lineBuffer, "%i %i %[^\n]s", &toWidth, &toHeight, fromFilePath)) {
    fprintf(stderr, "ERROR - line can not be parsed\n");
    return;
  }
  
  //
  // Try to open file.
  // If file can not be opened, fail via STDERR.
  //
  VipsImage *fromImage;
  // printf("fromFilePath %s", fromFilePath);
  if (!(fromImage = vips_image_new_from_file(fromFilePath, NULL))) {
    fprintf(stderr, "ERROR - Unable to open file\n");
    return;
  }
  
  //
  // Compute image size (width and height) and then use that information
  // to calculate horizontal and vertical scaling ratios.
  //
  int fromWidth = vips_image_get_width(fromImage);
  int fromHeight = vips_image_get_height(fromImage);
  double widthRatio = (double)toWidth / (double)fromWidth;
  double heightRatio = (double)fromHeight / (double)fromHeight;
  
  //
  // Actually resize image. If the return code is not 0 (success)
  // it is documented to be possible to be -1 (error), in that case
  // print something and faff around a bit
  //
  VipsImage *toImage;
  if (0 != vips_resize(fromImage, &toImage, widthRatio, heightRatio, NULL)) {
    fprintf(stderr, "ERROR - Unable to resize image\n");
    g_object_unref(fromImage);
    return;
  }
  
  //
  // Find a temporary file and open it using O_EXCL so it is guaranteed to be
  // written to only by this program. Hold the file descriptor open.
  // TBD: this may be wholly unnecessary as libVIPS author has worked on stream
  // support but it was in a different branch; there is no ability for libVIPS
  // savers to write to an open file descriptor, etc
  //
  char toFileTemplate[128] = "/tmp/converter-XXXXXX";
  if (!(mktemp(toFileTemplate))) {
    fprintf(stderr, "ERROR - Unable to open exclusive temporary file\n");
    g_object_unref(fromImage);
    g_object_unref(toImage);
    return;
  }
  
  //
  // Try to build a proper path name. Append .png so libVIPS knows to use the PNG
  // saver for it. Note: in the future this should be pluggable as part of the
  // informally-specified protocol in order to support PNG, JPEG and perhaps WebM.
  //
  char *toFileExtension = ".png";
  size_t toFileTemplateLength = strlen(toFileTemplate);
  size_t toFileExtensionLength = strlen(toFileExtension);
  char *toFilePath = (char *)malloc(toFileTemplateLength + toFileExtensionLength + 1);
  memcpy(toFilePath, toFileTemplate, toFileTemplateLength);
  memcpy(toFilePath + toFileTemplateLength, toFileExtension, toFileExtensionLength + 1);
  
  //
  // Save the file. Try to catch any sort of error if needed.
  //
  if (0 != vips_image_write_to_file(toImage, toFilePath, NULL)) {
    fprintf(stderr, "ERROR - Unable to write file\n");
    g_object_unref(fromImage);
    g_object_unref(toImage);
    free(toFilePath);
    return;
  }
  
  //
  // It would appear that the file has been saved properly now.
  //
  printf("%s\n", toFilePath);
  g_object_unref(fromImage);
  g_object_unref(toImage);
  free(toFilePath);
}
