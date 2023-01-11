import setuptools
from codecs import open
import os

wip_version = "0.4.2"

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
    name="pilotlight_tools",
    version=version_number(),
    license='MIT',
    python_requires='>=3.6',
    author="Jonathan Hoffstadt",
    author_email="pilot_light@yahoo.com",
    description='Pilot Light Tools',
    long_description=readme(),
    long_description_content_type="text/markdown",
    url='https://github.com/pilot-light/pilotlight', # Optional
    packages=['pilotlight_tools'],
    classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Education',
        'Intended Audience :: Developers',
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: MIT License',
        'Operating System :: MacOS :: MacOS X',
        'Operating System :: Microsoft :: Windows :: Windows 10',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Topic :: Software Development :: Libraries :: Python Modules',
    ],
    package_data={  # Optional
        'pilotlight_tools': ['README.md']
    }
)
