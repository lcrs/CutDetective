// Cut Detective looks at differences between frames
// to detect cuts, then writes an EDL which when
// relinked to the input clip has cuts on frames
// with large differences
//
// lewis@lewissaunders.com

#include <stdlib.h>
#include "half.h"
#include "spark.h"

int prevframeid;
int haveprev = 0;
unsigned long *savebuttoncallback(int what, SparkInfoStruct si);

// UI controls page 1, controls 6-34
//  6     13     20     27     34
//  7     14     21     28
//  8     15     22     29
//  9     16     23     30
//  10    17     24     31
//  11    18     25     32
//  12    19     26     33
SparkFloatStruct SparkFloat7 = {
  0.0,                          // Value
  -INFINITY,                    // Min
  +INFINITY,                    // Max
  0.1,                          // Increment
  0,                            // Flags
  (char *) "Difference %.2f",   // Title
  NULL                          // Callback
};
SparkFloatStruct SparkFloat8 = {
  2.0,                         // Value
  -INFINITY,                   // Min
  +INFINITY,                   // Max
  0.1,                         // Increment
  0,                           // Flags
  (char *) "Threshold %.2f",   // Title
  NULL                         // Callback
};
SparkStringStruct SparkString11 = {
	"/tmp/cutdetective.EDL",
	(char *) "Save as: %s",
	0,
	NULL
};
SparkIntStruct SparkInt25 = {
  24,                          // Value
  0,                           // Min
  99,                          // Max
  1,                           // Increment
  0,                           // Flags
  (char *) "FPS %2d",          // Title
  NULL                         // Callback
};
SparkPushStruct SparkPush32 = {
	(char *) "Save EDL",
	savebuttoncallback
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
  if(!bufferReady(prevframeid, &prevframe)) return(NULL);

  sparkCopyBuffer(front.Buffer, result.Buffer);

  return(result.Buffer);
}

// Called when Analyse button pressed
int SparkAnalyseStart(SparkInfoStruct si) {
	haveprev = 0;
  return 1;
}

// Spark entry point for each frame analysed
unsigned long *SparkAnalyse(SparkInfoStruct si) {
  // Check Spark image buffers are ready for use
  SparkMemBufStruct result, front, prev;
  if(!bufferReady(1, &result)) return(NULL);
  if(!bufferReady(2, &front)) return(NULL);
  if(!bufferReady(prevframeid, &prev)) return(NULL);

  sparkCopyBuffer(front.Buffer, result.Buffer);

	if(haveprev == 0) {
		// If this is the first frame of the analysis, we won't
		// have a previous frame buffer stored yet
	  sparkGetFrame(SPARK_FRONT_CLIP, si.FrameNo - 1, prev.Buffer);
		haveprev = 1;
	}

  float totaldifference = 0.0;
  for(int y = 0; y < front.BufHeight; y+=2) {
    for(int x = 0; x < front.BufWidth; x+=2) {
      char *frontpixel = (char *)(front.Buffer) + y * front.Stride + x * front.Inc;
      char *prevpixel = (char *)(prev.Buffer) + y * prev.Stride + x * prev.Inc;

      float frontfloat, prevfloat, difference;
      switch(front.BufDepth) {
        case SPARKBUF_RGB_24_3x8:
          frontfloat = *((unsigned char *)frontpixel);
          prevfloat = *((unsigned char *)prevpixel);
          difference = fabs(frontfloat - prevfloat) / 255.0;
          break;
        case SPARKBUF_RGB_48_3x10:
        case SPARKBUF_RGB_48_3x12:
          frontfloat = *((unsigned short *)frontpixel);
          prevfloat = *((unsigned short *)prevpixel);
          difference = fabs(frontfloat - prevfloat) / 65535.0;
          break;
        case SPARKBUF_RGB_48_3x16_FP:
          frontfloat = *((half *)frontpixel);
          prevfloat = *((half *)prevpixel);
          difference = fabs(frontfloat - prevfloat);
          break;
				default:
					break;
      }

      totaldifference += difference;
    }
  }

	if(si.FrameNo > 0) { // Don't set a key on the first frame, it might appear as a duplicate
	  float avgdifference = 100.0 * totaldifference / (front.BufWidth * front.BufHeight);
		SparkFloat7.Value = avgdifference;
		sparkSetCurveKey(SPARK_UI_CONTROL, 7, si.FrameNo + 1, avgdifference);
		sparkControlUpdate(7);
	}

	// Copy buffer for next frame
  sparkCopyBuffer(front.Buffer, prev.Buffer);

  return(result.Buffer);
}

// Called when Analyse pass finished
void SparkAnalyseEnd(SparkInfoStruct si) {
	haveprev = 0;

	// Set a key on the threshold so the line appears in the animation window
	float threshold0 = sparkGetCurveValuef(SPARK_UI_CONTROL, 8, 0);
	sparkSetCurveKey(SPARK_UI_CONTROL, 8, 0, threshold0);
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

// Called by Flame for assorted UI events
void SparkEvent(SparkModuleEvent e) {
  if(e == SPARK_EVENT_RESULT) {
    // For some reason the result view is not always updated when scrubbing in Front view then
    // hitting F4.  Shame because this makes F1/F4 Front/Result flipping slow
    sparkReprocess();
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

	FILE *fd = fopen(path, "w");
	fprintf(fd, "%s", "TITLE: CUT DETECTIVE\n");
	fprintf(fd, "%s", "FCM: NON-DROP FRAME\n");

	char *inpoint = (char *) malloc(13);
	char *outpoint = (char *) malloc(13);
	int eventno = 1;
	int prevoutpoint = 0;
	for(int i = 0; i < si.TotalFrameNo; i++) {
		float difference = sparkGetCurveValuef(SPARK_UI_CONTROL, 7, i);
		float threshold = sparkGetCurveValuef(SPARK_UI_CONTROL, 8, i);
		if(difference > threshold) {
			frame2tc(prevoutpoint, inpoint);
			frame2tc(i-1, outpoint);
			fprintf(fd, "%06d  MASTER  V  C  %s %s %s %s\n", eventno, inpoint, outpoint, inpoint, outpoint);
			eventno++;
			prevoutpoint = i-1;
		}
	}

	// Don't forget the last shot!
	frame2tc(prevoutpoint, inpoint);
	frame2tc(si.TotalFrameNo, outpoint);
	fprintf(fd, "%06d  MASTER  V  C  %s %s %s %s\n", eventno, inpoint, outpoint, inpoint, outpoint);

	free(inpoint);
	free(outpoint);

	fclose(fd);

	// Show a message in the interface
	char *m = (char *) calloc(1000, 1);
	strcat(m, "Saved detected cuts to ");
	strcat(m, path);
	sparkMessage(m);
	free(m);

	return NULL;
}
