import sys
import os
from urllib.request import urlretrieve
from zipfile import ZipFile

def reporthook(blocknum, blocksize, totalsize):
    bytesread = blocknum * blocksize
    if totalsize > 0:
        percent = bytesread * 1e2 / totalsize
        s = "\r%5.1f%% (%*d / %d bytes)" % (percent, len(str(totalsize)), bytesread, totalsize)
        sys.stderr.write(s)
        if bytesread >= totalsize:
            sys.stderr.write("\n")
    else:
        sys.stderr.write("read %d\n" % (bytesread,))

def download_zip(url, filename, description):  
    print("Downloading " + description + " from " + url)    
    urlretrieve(url, filename, reporthook)
    print("Download finished")
    print("Extracting")
    zip = ZipFile(filename, 'r')
    zip.extractall("../assets/")
    zip.close()
    os.remove(filename)

development_assets = False
gltf_assets = False

if len(sys.argv) > 1:
    target_directory = sys.argv[1]
    for i in range(1, len(sys.argv)):
        print(sys.argv[i])
        if sys.argv[i] == "development":
            development_assets = True
        elif sys.argv[i] == "gltf":
            gltf_assets = True
        else:
            target_directory = sys.argv[i]

if development_assets:
    download_zip('https://github.com/PilotLightTech/pilotlight-assets/archive/refs/heads/master.zip', '../assets/pilotlight-assets.zip', "test assets")
    os.rename('../assets/pilotlight-assets-master', '../assets/development')

if gltf_assets:
    download_zip('https://github.com/KhronosGroup/glTF-Sample-Assets/archive/refs/heads/main.zip', '../assets/gltf-sample-assets.zip', "sample gltf assets")
    os.rename('../assets/glTF-Sample-Assets-main', '../assets/gltf-samples')