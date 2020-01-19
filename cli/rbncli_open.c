#include "rbncli.h"

#ifdef _WIN32
#include <windows.h>
HMIDIIN hmi = NULL;
static void CALLBACK event_callback(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
  printf("%d\n", wMsg);
}
#endif

int rbncli_open_device(int argc, char** argv) {
  if(argc == 0) { // List available devices
#ifdef _WIN32
    UINT nMidiDeviceNum;
    nMidiDeviceNum = midiInGetNumDevs();
    if(nMidiDeviceNum == 0) {
      printf("No MIDI device detected\n");
      return -1;
    }

    printf("Devices:\n");
    MIDIINCAPS caps;
    for(UINT i = 0; i < nMidiDeviceNum; ++i) {
      midiInGetDevCaps(i, &caps, sizeof(MIDIINCAPS));
      printf("- %d: name = %s\n", i, caps.szPname);
    }
#endif
  } else {
#ifdef _WIN32
    UINT device_id = atoi(argv[0]);
    MMRESULT result = midiInOpen(&hmi, device_id, (DWORD_PTR)event_callback, (DWORD_PTR)NULL, CALLBACK_FUNCTION);
    if(result != MMSYSERR_NOERROR) {
      printf("Could not open MIDI device %d: %x\n", device_id, result);
      return -1;
    }
    midiInStart(hmi);

#endif
  }
  return 0;
}
