import shutil
import os
import SCons.Script

env = SCons.Script.DefaultEnvironment()

def copy_bin(source, target, env):
    shutil.copyfile(str(source[0]), "firmware.bin")
    print("---> Binary Successfully Copied to Root Directory!")

env.AddPostAction("$BUILD_DIR/firmware.bin", copy_bin)
