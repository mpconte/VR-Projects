/* Load sounds using the AudioToolbox */
#include "autocfg.h"
#include <stdio.h>
#include <stdlib.h>

#include <ve.h>

#define MODULE "ve_sndmac"

#if 1
VeSound *veSoundLoadFile_AudioToolbox(char *file) {
  static int init = 0;
  if (!init) {
    veNotice(MODULE,"No AudioToolbox/AudioFile support at this time");
    init =1;
  }
  return NULL;
}
#else
#ifndef HAS_AUDIOTOOLBOX
VeSound *veSoundLoadFile_AudioToolbox(char *file) {
  static int init = 0;
  if (!init) {
    /* show warning message once */
    veError(MODULE,"internal stumble:  tried to use AudioToolbox  but\n"
	    "configuration says no support - check veSoundLoadFile");
    veWarning(MODULE,"will return automatic failure for AudioToolbox");
    init = 1;
  }
  return NULL;
}
#else

#include <AudioToolBox/AudioToolbox.h>

static AudioFileID *gSourceAudioFileID;
static UInt64 gTotalPacketCount = 0;
static UInt64 gFileByteCount = 0;
static UInt32 gMaxPacketSize = 0;
static UInt64 gPacketOffset = 0;
static void *gSourceBuffer = NULL;

static OSStatus input_proc(AudioConverterRef inAudioConverter,
			   UInt32 *ioNumberDataPackets,
			   AudioBufferList *ioData,
			   AudioStreamPacketDescription **outDataPacketDescription,
			   void *inUserData) {
  UInt32 bytesReturned = 0;
  OSStatus err = noErr;

  ioData->mBuffers[0].mData = NULL;
  ioData->mBuffers[0].mDataByteSize = 0;

  if (gPacketOffset + *ioNumberDataPackets > gTotalPacketCount)
    *ioNumberDataPackets = gTotalPacketCount - gPacketOffset;

  if (*ioNumberDataPackets) {
    if (gSourceBuffer != NULL) {
      veFree(gSourceBuffer);
      gSourceBuffer = NULL;
    }
    gSourceBuffer = (void *)veAlloc(*ioNumberDataPackets * gMaxPacketSize,0);
    assert(gSourceBuffer != NULL);
    err = AudioFileReadPackets(*gSourceAudioFileID,false,&bytesReturned,
			       NULL,gPacketOffset,ioNumberDataPackets,
			       gSourceBuffer);
    if (err) {
      veError(MODULE,"failed reading packets for file...");
      return err;
    }
    
    gPacketOffset += *ioNumberDataPackets;
    ioData->mBuffers[0].mData = gSourceBuffer;
    ioData->mBuffers[0].mDataByteSize = bytesReturned;
  } else {
    /* end of data */
    ioData->mBuffers[0].mData = NULL;
    ioData->mBuffers[0].mDataByteSize = 0;
    err = eofErr;
  }

  if (err == eofErr && *ioNumberDataPackets)
    err = noErr;
  
  return err;
}

/* Return a structure - don't worry about id */
VeSound *veSoundLoadFile_AudioToolbox(char *file) {
  FSRef fsRef;
  Boolean isDir;
  AudioFileID afid;
  AudioStreamBasicDescription srcFmt, dstFmt;
  UInt32 psz;
  UInt64 numPackets, numBytes;
  AudioConverterRef converter;
  VeSound *sound;
  long fsz;
    
  fsz = veAudioGetFrameSize();

  if (FSPathMakeRef(file,&fsRef,&isDir) != noErr) {
    veError(MODULE,"failed to convert path '%s' to fsref",file);
    return NULL;
  }
  
  if (isDir) {
    veError(MODULE,"sound file '%s' is a directory",file);
    return NULL;
  }

  if (AudioFileOpen(&fsRef,fsRdPerm,0,&afid) != noErr) {
    veError(MODULE,"failed to open sound file '%s'",file);
    return NULL;
  }

  psz = sizeof(srcFmt);
  if (AudioFileGetProperty(afid,kAudioFilePropertyDataFormat,
			   &psz,&srcFmt) != noErr) {
    veError(MODULE,"could not query format of audio file '%s'",
	    file);
    AudioFileClose(afid);
    return NULL;
  }

  psz = sizeof(gTotalPacketCount);
  if (AudioFileGetProperty(afid,kAudioFilePropertyAudioDataPacketCount,
			   &psz,&gTotalPacketCount) != noErr) {
    veError(MODULE,"could not query number of packets in audio file '%s'",
	    file);
    AudioFileClose(afid);
    return NULL;
  }

  psz = sizeof(gMaxPacketSize);
  if (AudioFileGetProperty(afid,kAudioFilePropertyMaximumPacketSize,
			   &psz, &gMaxPacketSize)) {
    veError(MODULE,"could not query maximum packet size in audio file '%s'",
	    file);
    AudioFileClose(afid);
    return NULL;
  }

  gPacketOffset = 0;
  gSourceAudioFileID = &afid;

  /* determine "output" format */
  {
    dstFmt.mSampleRate = (Float64)veAudioGetSampFreq();
    dstFmt.mFormatID = kAudioFormatLinearPCM;
    dstFmt.mFormatFlags = kAudioFormatFlagIsFloat |
      kAudioFormatFlagsNativeEndian;
    dstFmt.mBytesPerFrame = fsz*sizeof(float);
    dstFmt.mFramesPerPacket = 1;
    /* ??? */
    dstFmt.mBytesPerPacket = dstFmt.mBytesPerFrame*dstFmt.mBytesPerPacket;
    dstFmt.mChannelsPerFrame = 1;
    dstFmt.mBitsPerChannel = 32;
    dstFmt.mReserved = 0;
  }

  /* build converter */
  if (AudioConverterNew(&srcFmt,&dstFmt,&converter) != noErr) {
    veError(MODULE,"could not create audio converter for file '%s'", file);
    AudioFileClose(afid);
    return NULL;
  }

  /* build memory buffer and read packets through converter
     to buffer */
  {
    UInt32 ioNumFrames;
    AudioBufferList outData;
    AudioStreamPacketDescription pdesc;

    sound = veAllocObj(VeSound);
    assert(sound != NULL);

    /* ... ??? ... */
    sound->nframes = ?;
    sound->data = ?;

    outData.mNumberBuffers = 1;
    outData.mBuffers[0].mNumberChannels = 1;
    outData.mBuffers[0].mDataByteSize <=


    ioNumFrames = 1;
    while (AudioConverterFillComplexBuffer(converter,
					   input_proc,
					   NULL,
					   &ioNumFrames,
					   &outData,
					   &pdesc) == noErr) {
      assert(ioNumFrames == 1); /* otherwise, there should be an error... */
      assert(outData.mNumberBuffers == 1);
      assert(outData.mBuffers[0].mNumberChannels == 1);
      assert(outData.mBuffers[0].mDataByteSize <= fsz*sizeof(float));
      
      /* append to buffer */
    }
  }
				
  AudioConverterDispose(converter);
  AudioFileClose(afid);

  return sound;
}
#endif /* HAS_AUDIOTOOLBOX */
#endif /* 1 */
