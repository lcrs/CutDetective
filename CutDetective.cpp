// Cut Detective looks at differences between frames
// to detect cuts, then writes an EDL which when
// relinked to the input clip has cuts on frames
// with large differences
//
// TODO:
//  o Checkerboard shots into two EDLs, to make commiting shots
//    with duplicate frames easier?
//  o Worth multithreading the difference loop?
//
// lewis@lewissaunders.com

#include <stdlib.h>
#include <libgen.h>
#include "half.h"
#include "spark.h"

// ID of Spark buffer we use to store previous frame
int prevframeid;

// We store each frame in a malloc()'d buffer here for retrieval on the next frame
void *prevframebuffer;

// Whether previous frame is available already
int haveprev = 0;

// Forward declare callback function for save button click
unsigned long *savebuttoncallback(int what, SparkInfoStruct si);

// UI controls page 1, controls 6-34
//  6     13     20     27     34
//  7     14     21     28
//  8     15     22     29
//  9     16     23     30
//  10    17     24     31
//  11    18     25     32
//  12    19     26     33
SparkFloatStruct SparkFloat21 = {
  0.0,                          // Value
  -INFINITY,                    // Min
  +INFINITY,                    // Max
  0.1,                          // Increment
  SPARK_FLAG_NO_INPUT,          // Flags
  (char *) "Current difference %.2f",   // Title
  NULL                          // Callback
};
SparkBooleanStruct SparkBoolean15 = {
  1,
  (char *) "Detect cuts",
  NULL
};
SparkFloatStruct SparkFloat22 = {
  8.0,                         // Value
  -INFINITY,                   // Min
  +INFINITY,                   // Max
  0.1,                         // Increment
  0,                           // Flags
  (char *) "Cut threshold %.2f",   // Title
  NULL                         // Callback
};
SparkBooleanStruct SparkBoolean16 = {
  0,
  (char *) "Remove duplicate frames",
  NULL
};
SparkFloatStruct SparkFloat23 = {
  0.20,                        // Value
  -INFINITY,                   // Min
  +INFINITY,                   // Max
  0.1,                         // Increment
  0,                           // Flags
  (char *) "Duplicate threshold %.2f",   // Title
  NULL                         // Callback
};
SparkStringStruct SparkString11 = {
	"/tmp/cutdetective.edl",
	(char *) "Save as: %s",
	0,
	NULL
};
SparkIntStruct SparkInt25 = {
  24,                          // Value
  0,                           // Min
  99,                          // Max
  1,                           // Increment
  SPARK_FLAG_NO_ANIM,          // Flags
  (char *) "FPS %2d",          // Title
  NULL                         // Callback
};
SparkPushStruct SparkPush32 = {
	(char *) "Save EDL",
	savebuttoncallback
};
SparkIntStruct SparkSetupInt15 = {
  8,
  1,
  128,
  1,
  SPARK_FLAG_NO_ANIM,
  (char *) "Downres factor: %d",
  NULL
};

// Check that a Spark image buffer is ready to use
int bufferReady(int id, SparkMemBufStruct *b) {
  if(!sparkMemGetBuffer(id, b)) {
    printf("CutDetective: Failed to get buffer %d\n", id);
    return 0;
  }
  if(!(b->BufState & MEMBUF_LOCKED)) {
    printf("CutDetective: Failed to lock buffer %d\n", id);
    return 0;
  }
  return 1;
}

// Flame asks us what extra image buffers we'll want here, we register 1
void SparkMemoryTempBuffers(void) {
    prevframeid = sparkMemRegisterBuffer();
}

// Spark entry function
unsigned int SparkInitialise(SparkInfoStruct si) {
  return(SPARK_MODULE);
}

// Spark entry point for each frame to be rendered
unsigned long *SparkProcess(SparkInfoStruct si) {
  // Check Spark image buffers are ready for use
  SparkMemBufStruct result, front, prevframe;
  if(!bufferReady(1, &result)) return(NULL);
  if(!bufferReady(2, &front)) return(NULL);

  sparkCopyBuffer(front.Buffer, result.Buffer);

  return(result.Buffer);
}

// Spark entry point for each frame analysed
unsigned long *SparkAnalyse(SparkInfoStruct si) {
  // Check Spark image buffers are ready for use
  SparkMemBufStruct result, front, prev;
  if(!bufferReady(1, &result)) {
    printf("CutDetective: result buffer not ready at frame %d!\n", si.FrameNo);
    return(NULL);
  }
  if(!bufferReady(2, &front)) {
    printf("CutDetective: front buffer not ready at frame %d!\n", si.FrameNo);
    return(NULL);
  }

	if(haveprev == 0) {
		// If this is the first frame of the analysis, we won't
		// have a previous frame buffer stored yet, so fetch it
    if(!bufferReady(prevframeid, &prev)) {
      printf("CutDetective: prev buffer not ready at frame %d!\n", si.FrameNo);
      return(NULL);
    }
	  sparkGetFrame(SPARK_FRONT_CLIP, si.FrameNo - 1, prev.Buffer);
    prevframebuffer = malloc(si.FrameBytes);
    memcpy(prevframebuffer, prev.Buffer, si.FrameBytes);
    haveprev = 1;
	}

  // Loop through pixels, find difference to same pixel
  // in previous frame, and sum up the differences
  float totaldifference = 0.0;
  int downres = SparkSetupInt15.Value;
  for(int y = 0; y < (front.BufHeight - downres); y += downres) {
    for(int x = 0; x < (front.BufWidth - downres); x += downres) {
      char *frontpixel = (char *)(front.Buffer) + y * front.Stride + x * front.Inc;
      char *prevpixel = (char *)(prevframebuffer) + y * front.Stride + x * front.Inc;

      float r, g, b, l, prevr, prevg, prevb, prevl, difference;
      switch(front.BufDepth) {
        case SPARKBUF_RGB_24_3x8:
          r = *(unsigned char *)(frontpixel + 0) / 255.0;
          g = *(unsigned char *)(frontpixel + 1) / 255.0;
          b = *(unsigned char *)(frontpixel + 2) / 255.0;
          prevr = *(unsigned char *)(prevpixel + 0) / 255.0;
          prevg = *(unsigned char *)(prevpixel + 1) / 255.0;
          prevb = *(unsigned char *)(prevpixel + 2) / 255.0;
          break;
        case SPARKBUF_RGB_48_3x10:
        case SPARKBUF_RGB_48_3x12:
          r = *(unsigned short *)(frontpixel + 0) / 65535.0;
          g = *(unsigned short *)(frontpixel + 2) / 65535.0;
          b = *(unsigned short *)(frontpixel + 4) / 65535.0;
          prevr = *(unsigned short *)(prevpixel + 0) / 65535.0;
          prevg = *(unsigned short *)(prevpixel + 2) / 65535.0;
          prevb = *(unsigned short *)(prevpixel + 4) / 65535.0;
          break;
        case SPARKBUF_RGB_48_3x16_FP:
          r = *(half *)(frontpixel + 0);
          g = *(half *)(frontpixel + 2);
          b = *(half *)(frontpixel + 4);
          prevr = *(half *)(prevpixel + 0);
          prevg = *(half *)(prevpixel + 2);
          prevb = *(half *)(prevpixel + 4);
          break;
				default:
					break;
      }

      // Rec709 luma weights
      l = 0.2126 * r + 0.7152 * g + 0.0722 * b;
      prevl = 0.2126 * prevr + 0.7152 * prevg + 0.0722 * prevb;
      difference = fabs(l - prevl);
      totaldifference += difference;
    }
  }

  // Set difference key for this frame
  float avgdifference = 100.0 * totaldifference / ((front.BufWidth/downres) * (front.BufHeight/downres));
	SparkFloat21.Value = avgdifference;
	sparkSetCurveKey(SPARK_UI_CONTROL, 21, si.FrameNo + 1, avgdifference);
	sparkControlUpdate(21);

	// Copy buffer for next frame
  memcpy(prevframebuffer, front.Buffer, si.FrameBytes);

  return(front.Buffer);
}

// Called when Analyse pass finished
void SparkAnalyseEnd(SparkInfoStruct si) {
  printf("Analyse end at frame %d\n", si.FrameNo);

  free(prevframebuffer);
	haveprev = 0;

	// Set a key on the thresholds so the lines appear in the animation window
	float cutthreshold0 = sparkGetCurveValuef(SPARK_UI_CONTROL, 22, 0);
	sparkSetCurveKey(SPARK_UI_CONTROL, 22, 0, cutthreshold0);
	float dupthreshold0 = sparkGetCurveValuef(SPARK_UI_CONTROL, 23, 0);
	sparkSetCurveKey(SPARK_UI_CONTROL, 23, 0, dupthreshold0);
}

// Spark deletion entry point
void SparkUnInitialise(SparkInfoStruct si) {
}

// Called by Flame to find out what bit-depths we support... all of them :)
int SparkIsInputFormatSupported(SparkPixelFormat fmt) {
  switch(fmt) {
    case SPARKBUF_RGB_24_3x8:
    case SPARKBUF_RGB_48_3x10:
    case SPARKBUF_RGB_48_3x12:
    case SPARKBUF_RGB_48_3x16_FP:
      return 1;
      break;
    default:
      return 0;
  }
}

// Called by Flame to find out how many input clips we require
int SparkClips(void) {
  return 1;
}

// Convert frame count to timecode
// No drop-frame support, fps must be an integer!
void frame2tc(int i, char *tc) {
	int fps = SparkInt25.Value;
	int h = floor(i/60.0/60.0/fps);
	int m = floor((i-h*60.0*60.0*fps) / 60.0 / fps);
	int s = floor((i-h*60.0*60.0*fps - m*60.0*fps) / fps);
	int f = floor(i-h*60.0*60.0*fps - m*60.0*fps - s * fps);
	sprintf(tc, "%02d:%02d:%02d:%02d", h, m, s, f);
}

// Save EDL... button is clicked
unsigned long *savebuttoncallback(int what, SparkInfoStruct si) {
	char *path = strdup(SparkString11.Value);

	// Sometimes strings from UI controls come back with a line break
	int pathlen = strlen(path);
	if(path[pathlen - 1] == '\n') {
		path[pathlen - 1] = '\0';
	}

  // basename() is within its rights to trash its input
  char *pathdup = strdup(path);
  char *base = basename(pathdup);

	FILE *fd = fopen(path, "w");
	fprintf(fd, "TITLE: Cut Detective %s\n", base);
	fprintf(fd, "%s", "FCM: NON-DROP FRAME\n");

	char *sourcein = (char *) malloc(13);
	char *sourceout = (char *) malloc(13);
  char *recordin = (char *) malloc(13);
  char *recordout = (char *) malloc(13);
  char *removedtc = (char *) malloc(13);
  char *cuttc = (char *) malloc(13);
	int eventno = 1;
	int prevoutpoint = 0;
  int removed = 0;
  int cuts = 0;
	for(int i = 1; i < si.TotalFrameNo; i++) {
		float difference = sparkGetCurveValuef(SPARK_UI_CONTROL, 21, i);
		float cutthreshold = sparkGetCurveValuef(SPARK_UI_CONTROL, 22, i);
		float dupthreshold = sparkGetCurveValuef(SPARK_UI_CONTROL, 23, i);
		if(SparkBoolean15.Value == 1 && difference > cutthreshold) {
      // This frame is the first frame of a new shot, write EDL event for
      // the shot that just finished
			frame2tc(prevoutpoint, sourcein);
			frame2tc(i - 1, sourceout);
			frame2tc(prevoutpoint - removed, recordin);
			frame2tc(i - (removed + 1), recordout);
      if(prevoutpoint != i - 1) {
        // Only write an event if it wouldn't be zero-length
        fprintf(fd, "\n%06d  MASTER  V  C  %s %s %s %s\n", eventno, sourcein, sourceout, recordin, recordout);
  			eventno++;
        cuts++;
      }
      frame2tc(i, cuttc);
      fprintf(fd, "At end of this shot CutDetective detected a cut at source frame %d, %s\n", i, cuttc);
			prevoutpoint = i - 1; // Next shot should start on this frame, i.e. a match-cut
		}
    if(SparkBoolean16.Value == 1 && difference < dupthreshold && i > 1) {
      if(prevoutpoint == i - 1) {
        // We already just finished a shot, don't write a zero-length event
        // This happens if we're removing multiple dupes in a row
        removed++;
        prevoutpoint = i;
        continue;
      }
      // This frame needs to be removed, write EDL event for shot that just
      // finished
			frame2tc(prevoutpoint, sourcein);
			frame2tc(i - 1, sourceout);
			frame2tc(prevoutpoint - removed, recordin);
			frame2tc(i - (removed + 1), recordout);
			fprintf(fd, "\n%06d  MASTER  V  C  %s %s %s %s\n", eventno, sourcein, sourceout, recordin, recordout);
      frame2tc(i, removedtc);
      fprintf(fd, "At end of this shot CutDetective removed duplicate source frames at %d, %s\n", i, removedtc);
      eventno++;
      removed++;
      prevoutpoint = i; // Next shot should start on the next frame, not this one
    }
	}

	// Don't forget the last shot!
  int i = si.TotalFrameNo + 1;
  frame2tc(prevoutpoint, sourcein);
  frame2tc(i - 1, sourceout);
  frame2tc(prevoutpoint - removed, recordin);
  frame2tc(i - (removed + 1), recordout);
  fprintf(fd, "\n%06d  MASTER  V  C  %s %s %s %s\n", eventno, sourcein, sourceout, recordin, recordout);
  fprintf(fd, "At end of this shot CutDetective reached end of source\n");

	free(sourcein);
	free(sourceout);
  free(recordin);
  free(recordout);
  free(removedtc);
  free(cuttc);
  free(pathdup);

	fclose(fd);

	// Show a message in the interface
	char *m = (char *) calloc(1000, 1);
  float avglen = (float)(i - removed - 1) / (cuts+1);
  sprintf(m, "%d cuts in %s, average %.1f fr, removed %d duplicates", cuts, path, avglen, removed);
	sparkMessage(m);
	free(m);

	return NULL;
}
