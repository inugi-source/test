package org.chromium.alloy.adb;

/**
 * TODO: Insert description here. (generated by penghuang)
 */
class JDWPService extends AdbSocket {
  private int mPass = 0;

  private String listProcess() {
    // TODO(penghuang): returns processes.
    return "";
  }
  
  @Override
  public void ready() {
    if (mPass == 0) {
      AdbMessage message = new AdbMessage();
      message.setData(listProcess());
      mPeer.enqueue(message);
      mPass++;
    } else {
      mPeer.close();
    }
  }
}

class TrackJDWPService extends AdbSocket {
  private boolean mNeedUpdate = true;
  
  private String listProcess() {
    // TODO(penghuang): returns processes.
    return "0000";
  }

  @Override
  public void ready() {
    if (mNeedUpdate) {
      AdbMessage message = new AdbMessage();
      message.setData(listProcess());
      mPeer.enqueue(message);      
      mNeedUpdate = false;
    }
  }
}