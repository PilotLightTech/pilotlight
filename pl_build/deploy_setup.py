import setuptools
from codecs import open
import os

wip_version = "1.0.11"

def version_number():
    """This function reads the version number which is populated by github actions"""

    if os.environ.get('READTHEDOCS') == 'True':
        return wip_version
    try:
        with open('version_number.txt', encoding='utf-8') as f:
            return f.readline().rstrip()

    except IOError:
        return wip_version

def readme():
    try:
        with open('README.md', encoding='utf-8') as f:
            return f.read()
    except IOError:
        return 'Not Found'

setuptools.setup(
    name="pl_build",
    version=version_number(),
    license='MIT',
    python_requires='>=3.6',
    author="Jonathan Hoffstadt",
    author_email="jonathanhoffstadt@yahoo.com",
    description='Pilot Light Build',
    long_description=readme(),
    long_description_content_type="text/markdown",
    url='https://github.com/PilotLightTech/pilotlight', # Optional
    packages=['pl_build'],
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Operating System :: MacOS :: MacOS X',
        'Operating System :: Microsoft :: Windows :: Windows 10',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Topic :: Software Development :: Libraries :: Python Modules',
    ],
    package_data={  # Optional
        'pl_build': ['README.md']
    }
)
