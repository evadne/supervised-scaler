// 
// VIPS Resampler (C Server).
// Portions from VIPS Thumbnailer, https://github.com/jcupitt/libvips/blob/master/tools/vipsthumbnail.c
// 

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <vips/vips.h>

//
// § Definitions
//

// 
// Linear vs. Perceptual. It would seem that linear vs. perceptual space is
// not a concern, since this program’s output will be solely used in a browser
// and not in further applications downstream.
// 
#define LINEAR_PROCESSING 0

// 
// Processing / Exporting Colour Space. Currently the only use case is for Web so everything
// gets exported in sRGB (if not already in that colour space).
// 
#define PROCESSING_COLOUR_SPACE (LINEAR_PROCESSING ? VIPS_INTERPRETATION_scRGB : VIPS_INTERPRETATION_sRGB)
#define EXPORTING_COLOUR_SPACE VIPS_INTERPRETATION_sRGB

// 
// Template for the temporary file. Presumablty if the program is to be ported elsewhere
// it will need to vary. Also it is plausible that the end user may want to customise this on their own as well
// 
#define TEMPORARY_FILE_PATH_TEMPLATE "/tmp/converter-XXXXXX.jpg"

// 
// Define whether memory can be used between passes. By default this means a 100MB high water mark
// within each pass and no cached information to persist between passes. In the future this can be made tweakable
// since code can be deployed in many places. Also threading should be tweakable.
// 
#define CACHE_BYTES_ALLOWED_WITHIN_EACH_PASS 104857600
#define CACHE_BYTES_ALLOWED_BETWEEN_EACH_PASS 0

//
// § Shrink Factor Calculation
//

double shrinkFactorForImage (VipsImage *fromImage, VipsAngle fromAngle, double toWidth, double toHeight) {
  const double fromWidth = (double)vips_image_get_width(fromImage);
  const double fromHeight = (double)vips_image_get_height(fromImage);
  switch (fromAngle) {
    case VIPS_ANGLE_D90:
    case VIPS_ANGLE_D270:
      return fmin((fromHeight / toWidth), (fromWidth / toHeight));
    default:
      return fmin((fromWidth / toWidth), (fromHeight / toHeight));
  }
}

unsigned int jpegShrinkFactorForFactor (double shrinkFactor) {
  if (LINEAR_PROCESSING) {
    return 1;
  } else {
    if (shrinkFactor >= 16) {
      return 8;
    } else if (shrinkFactor >= 8) {
      return 4;
    } else if (shrinkFactor >= 4) {
      return 2;
    } else {
      return 1;
    }
  }
}

void getImageShrinkFactorAndAngle (VipsImage *image, unsigned int toWidth, unsigned int toHeight, double *outShrinkFactor, VipsAngle *outAngle) {
  *outAngle = vips_autorot_get_angle(image);
  *outShrinkFactor = shrinkFactorForImage(image, *outAngle, (double)toWidth, (double)toHeight);
}

//
// § Image Loading
//

VipsImage * newImageFromGenericPath (char *filePath, unsigned int toWidth, unsigned int toHeight, double *outShrinkFactor, VipsAngle *outAngle) {
  VipsImage *loadedImage = vips_image_new_from_file(filePath, "access", VIPS_ACCESS_SEQUENTIAL, NULL);
  getImageShrinkFactorAndAngle(loadedImage, toWidth, toHeight, outShrinkFactor, outAngle);
  return loadedImage;
}

VipsImage * newImageFromJPEGPath (char *filePath, unsigned int toWidth, unsigned int toHeight, double *outShrinkFactor, VipsAngle *outAngle) {
  VipsImage *headerImage = newImageFromGenericPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
  g_object_unref(headerImage);
  
  VipsImage *loadedImage = vips_image_new_from_file(filePath, "access", VIPS_ACCESS_SEQUENTIAL, "shrink", jpegShrinkFactorForFactor(*outShrinkFactor), NULL);
  getImageShrinkFactorAndAngle(loadedImage, toWidth, toHeight, outShrinkFactor, outAngle);
  return loadedImage;
}

VipsImage * newImageFromPDFPath (char *filePath, unsigned int toWidth, unsigned int toHeight, double *outShrinkFactor, VipsAngle *outAngle) {
  VipsImage *headerImage = newImageFromGenericPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
  g_object_unref(headerImage);
  
  VipsImage *loadedImage = vips_image_new_from_file(filePath, "access", VIPS_ACCESS_SEQUENTIAL, "scale", (1.0 / (*outShrinkFactor)), NULL);
  getImageShrinkFactorAndAngle(loadedImage, toWidth, toHeight, outShrinkFactor, outAngle);
  return loadedImage;
}

VipsImage * newImageFromSVGPath (char *filePath, unsigned int toWidth, unsigned int toHeight, double *outShrinkFactor, VipsAngle *outAngle) {
  //
  // Same underlying code path as PDF loading.
  //
  return newImageFromPDFPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
}

VipsImage * newImageFromWebPPath (char *filePath, unsigned int toWidth, unsigned int toHeight, double *outShrinkFactor, VipsAngle *outAngle) {
  VipsImage *headerImage = newImageFromGenericPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
  g_object_unref(headerImage);
  
  VipsImage *loadedImage = vips_image_new_from_file(filePath, "access", VIPS_ACCESS_SEQUENTIAL, "shrink", *outShrinkFactor, NULL);
  getImageShrinkFactorAndAngle(loadedImage, toWidth, toHeight, outShrinkFactor, outAngle);
  return loadedImage;
}

VipsImage * newImageFromPath (char *filePath, unsigned int toWidth, unsigned int toHeight, double *outShrinkFactor, VipsAngle *outAngle) {
  const char *loaderName = vips_foreign_find_load(filePath);
  
  if (!loaderName) {
    return NULL;
  }
  
  if (0 == strcmp(loaderName, "VipsForeignLoadJpegFile")) {
    return newImageFromJPEGPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
  }
  
  if (0 == strcmp(loaderName, "VipsForeignLoadPdfFile")) {
    return newImageFromPDFPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
  }
  
  if (0 == strcmp(loaderName, "VipsForeignLoadSvgFile")) {
    return newImageFromSVGPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
  }
  
  if (0 == strcmp(loaderName, "VipsForeignLoadWebpFile")) {
    return newImageFromWebPPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
  }
  
  return newImageFromGenericPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
}

//
// § Image Shrinking
//

bool shouldImportColourProfileForImage (VipsImage *image) {
  const VipsInterpretation fromInterpretation = vips_image_get_interpretation(image);
  const VipsCoding fromCoding = vips_image_get_coding(image);
  const VipsBandFormat fromBandFormat = vips_image_get_format(image);
  
  if (LINEAR_PROCESSING || (fromInterpretation == VIPS_INTERPRETATION_CMYK)) {
    if (fromCoding == VIPS_CODING_NONE) {
      if ((fromBandFormat == VIPS_FORMAT_UCHAR) || (fromBandFormat == VIPS_FORMAT_USHORT)) {
        if (vips_image_get_typeof(image, VIPS_META_ICC_NAME)) {
          return true;
        }
      }
    }
  }
  
  return false;
}

bool shouldConvertImageToProcessingColourSpace (VipsImage *image) {
  const VipsInterpretation fromInterpretation = vips_image_get_interpretation(image);
  const VipsInterpretation toInterpretation = PROCESSING_COLOUR_SPACE;
  
  if (fromInterpretation != toInterpretation) {
    return true;
  } else {
    return false;
  }
}

bool shouldPremultiplyImage (VipsImage *image) {
  //
  // Check whether the image has an alpha channel; if it does then it needs to be pre-multiplied
  // before being shrunk, then un-premultiplied later after being shrunk.
  //
  // Note: VIPS Thumbnailer implements this in the same way by checking the number of bands
  // but really the function should be able to tell whether the image has an alpha channel
  // in order to return a proper answer. So we’re winging it at the moment.
  //
  const int fromNumberOfBands = vips_image_get_bands(image);
  
  if (fromNumberOfBands == 2) {
    return true;
  }
  
  if (fromNumberOfBands == 4) {
    const VipsInterpretation fromInterpretation = vips_image_get_interpretation(image);
    if (fromInterpretation != VIPS_INTERPRETATION_CMYK) {
      return true;
    }
  }
  
  if (fromNumberOfBands == 5) {
    return true;
  }
  
  return false;
}

bool shouldConvertImageToExportingColourSpace (VipsImage *image) {
  const VipsInterpretation fromInterpretation = vips_image_get_interpretation(image);
  const VipsInterpretation toInterpretation = EXPORTING_COLOUR_SPACE;
  
  if (fromInterpretation != toInterpretation) {
    return true;
  } else {
    return false;
  }
}

VipsImage *newThumbnailFromImage (VipsObject *context, VipsImage *parentImage, VipsAngle angle, double shrinkFactor) {
  VipsImage **localImages = (VipsImage **)vips_object_local_array(context, 10);
  VipsImage *currentImage = parentImage;
  
  bool havePremultiplied = false;
  VipsBandFormat imageBandFormatBeforePremultiplication = VIPS_FORMAT_NOTSET;
  
  //
  // 01 § Special unpacking for RAD
  //
  if (VIPS_CODING_RAD == vips_image_get_coding(currentImage)) {
    VipsImage *unpackedImage = NULL;
    if (vips_rad2float(currentImage, &unpackedImage, NULL)) {
      return NULL;
    }
    currentImage = localImages[0] = unpackedImage;
  }
    
  //
  // 02 § Import colour profile, if needed
  //
  if (shouldImportColourProfileForImage(currentImage)) {
    VipsImage *importedImage = NULL;
    if (vips_icc_import(currentImage, &importedImage, "embedded", TRUE, "pcs", VIPS_PCS_XYZ, NULL)) {
      return NULL;
    }
    currentImage = localImages[1] = importedImage;
  }
  
  //
  // 03 § Convert to device colour space, if needed
  //
  if (shouldConvertImageToProcessingColourSpace(currentImage)) {
    VipsImage *convertedImage = NULL;
    if (vips_colourspace(currentImage, &convertedImage, PROCESSING_COLOUR_SPACE, NULL)) {
      return NULL;
    }
    currentImage = localImages[2] = convertedImage;
  }
  
  //
  // 04 § Premultiply image, if needed
  //
  if (shouldPremultiplyImage(currentImage)) {
    imageBandFormatBeforePremultiplication = vips_image_get_format(currentImage);
    VipsImage *premultipliedImage = NULL;
    if (vips_premultiply(currentImage, &premultipliedImage, NULL)) {
      return NULL;
    }
    havePremultiplied = true;
    currentImage = localImages[3] = premultipliedImage;
  }
  
  //
  // 05 § Resize image
  //
  VipsImage *resizedImage = NULL;
  if (vips_resize(currentImage, &resizedImage, (1.0 / shrinkFactor), "centre", TRUE, NULL)) {
    return NULL;
  }
  currentImage = localImages[4] = resizedImage;
  
  //
  // 06 § Unpremultiply image if premultiplied earlier
  // 07 § Cast image back to original band format if premultiplied earlier
  //
  if (havePremultiplied) {
    VipsImage *unpremultipliedImage = NULL;
    if (vips_unpremultiply(currentImage, &unpremultipliedImage, NULL)) {
      return NULL;
    }
    currentImage = localImages[5] = unpremultipliedImage;
    
    VipsImage *castImage = NULL;
    if (vips_cast(currentImage, &castImage, imageBandFormatBeforePremultiplication, NULL)) {
      return NULL;
    }
    currentImage = localImages[6] = unpremultipliedImage;
  }
  
  //
  // 08 § Transform image back to exporting colour space, if needed
  //
  if (shouldConvertImageToExportingColourSpace(currentImage)) {
    VipsImage *exportedImage = NULL;
    if (vips_colourspace(currentImage, &exportedImage, EXPORTING_COLOUR_SPACE, NULL)) {
      return NULL;
    }
    currentImage = localImages[7] = exportedImage;
  }
  
  //
  // 09 § Rotate image, if needed
  //
  if (angle != VIPS_ANGLE_D0) {
    VipsImage *copiedImage = localImages[8] = vips_image_new_memory();
    VipsImage *rotatedImage = NULL;
    if (vips_image_write(currentImage, copiedImage) || vips_rot(copiedImage, &rotatedImage, angle, NULL)) {
      return NULL;
    }
    currentImage = localImages[9] = rotatedImage;
    vips_autorot_remove_angle(currentImage);
  }
  
  //
  // 10 § Strip colour profile, if needed; let devices fall back to sRGB
  //
  if (vips_image_get_typeof(currentImage, VIPS_META_ICC_NAME)) {
    if (!vips_image_remove(currentImage, VIPS_META_ICC_NAME)) { 
      return NULL;
    }
  }
  
  return currentImage;
}

//
// § Saving
//

char * pathWithWrittenDataForImage(VipsImage *image) {
  //
  // Find a temporary file and create it directly using mkstemps,
  // add a path extension so libVIPS knows which format to use.
  // Note: in the future we should allow configuring the server for PNG, JPEG and perhaps WebM.
  //
  static char * const fileTemplate = TEMPORARY_FILE_PATH_TEMPLATE;
  char *filePath = (char *)malloc(strlen(fileTemplate) + 1);
  strcpy(filePath, fileTemplate);
  
  if (!(mkstemps(filePath, 4))) {
    fprintf(stderr, "ERROR - Unable to open exclusive temporary file\n");
    free(filePath);
    return NULL;
  }
  
  //
  // Save the file. Try to catch any sort of error if needed.
  //
  if (0 != vips_image_write_to_file(image, filePath, "strip", TRUE, NULL)) {
    return NULL;
  }
  
  return filePath;
}

//
// § Glue
//

void processLine (char *lineBuffer, VipsObject *context) {
  //
  // Attempt to scan information from input string. Input must have the format of
  // <toWidth> <toHeight> <fromPath> and is supposed to be using a path that is within
  // a controlled temporary directory and a server-generated file name, instead of anywhere
  // in the filesystem and/or an user-provided file name.
  //
  char fromFilePath[4096];
  unsigned int toWidth = 0;
  unsigned int toHeight = 0;
  if (3 != sscanf(lineBuffer, "%u %u %[^\n]s", &toWidth, &toHeight, fromFilePath)) {
    fprintf(stderr, "ERROR - line can not be parsed\n");
    return;
  }
  
  //
  // Try to open file; if shrink-on-load is supported load a smaller version immediately.
  // otherwise, load the full version later but still compute angle and shrink factor.
  //
  // If file can not be opened, fail.
  //
  VipsAngle fromAngle = VIPS_ANGLE_D0;
  double shrinkFactor = 1.0;
  VipsImage *fromImage = newImageFromPath(fromFilePath, toWidth, toHeight, &shrinkFactor, &fromAngle);
  if (!fromImage) {
    fprintf(stderr, "ERROR - Unable to open file\n");
    return;
  }
  vips_object_local(context, fromImage);
  
  //
  // The fun bit. Make thumbnail.
  //
  VipsImage *thumbnailImage = newThumbnailFromImage(context, fromImage, fromAngle, shrinkFactor);
  if (!thumbnailImage) {
    fprintf(stderr, "ERROR - Unable to resize image\n");
    return;
  }
  
  char *toFilePath = pathWithWrittenDataForImage(thumbnailImage);
  if (toFilePath) {
    printf("%s\n", toFilePath);
    free(toFilePath);
  } else {
    fprintf(stderr, "ERROR - Unable to write file\n");
  }
}

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
  // Note: fgets blocks, without burning CPU. Looks like the best function on macOS;
  // other ones burn CPU when idle.
  //
  char lineBuffer[4096];
  while (fgets(lineBuffer, 4096, stdin)) {
    //
    // Start the main process loop. Spin up a VipsObject in order for
    // downstream functions to hang all dependent objects on this context object,
    // so it can all be de-allocated in one go later on.
    //
    VipsObject *context = VIPS_OBJECT(vips_image_new()); 
    processLine(lineBuffer, context);
    vips_error_clear();
    g_object_unref(context);
    
    //
    // Empty cache but allow a higher high water mark within each pass,
    // if so desired. By default this is set to 0MB and 100MB respectively
    // but can be tweaked in the future.
    //
    vips_cache_set_max_mem(CACHE_BYTES_ALLOWED_BETWEEN_EACH_PASS);
    vips_cache_set_max_mem(CACHE_BYTES_ALLOWED_WITHIN_EACH_PASS);
  }
  
  vips_shutdown();
  return 0;
}
