var Module = typeof Module !== "undefined" ? Module : {};

Module.preRun = Module.preRun || [];
Module.bt3dStorageSyncing = false;
Module.bt3dStorageSyncPending = false;
Module.bt3dSyncPersistentStorage = function() {
  if (typeof FS === "undefined" || !FS.syncfs) return;
  if (Module.bt3dStorageSyncing) {
    Module.bt3dStorageSyncPending = true;
    return;
  }
  Module.bt3dStorageSyncing = true;
  FS.syncfs(false, function(err) {
    Module.bt3dStorageSyncing = false;
    if (err) {
      console.error("bt3d: IDBFS sync failed", err);
    }
    if (Module.bt3dStorageSyncPending) {
      Module.bt3dStorageSyncPending = false;
      Module.bt3dSyncPersistentStorage();
    }
  });
};

Module.preRun.push(function() {
  var mountPoint = "/bt3d";
  Module.addRunDependency("bt3d-idbfs");
  try {
    if (!FS.analyzePath(mountPoint).exists) {
      FS.mkdir(mountPoint);
    }
    FS.mount(IDBFS, { autoPersist: false }, mountPoint);
    FS.syncfs(true, function(err) {
      if (err) {
        console.error("bt3d: failed to populate IDBFS", err);
      }
      Module.removeRunDependency("bt3d-idbfs");
    });
  } catch (err) {
    console.error("bt3d: failed to mount IDBFS", err);
    Module.removeRunDependency("bt3d-idbfs");
  }
});
