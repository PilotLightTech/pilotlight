import sys
import os
from urllib.request import urlretrieve
from zipfile import ZipFile

if not os.path.isdir('../data'):
    os.mkdir('../data')

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
    zip.extractall("../data/")
    zip.close()
    os.remove(filename)

download_zip('https://github.com/KhronosGroup/glTF-Sample-Models/archive/refs/heads/master.zip', '../data/gltf-sample-models.zip', "sample gltf models")
download_zip('https://github.com/KhronosGroup/glTF-Sample-Environments/archive/refs/heads/master.zip', '../data/gltf-sample-environments.zip', "sample environments")