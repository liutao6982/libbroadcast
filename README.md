Libbroadcast
============

Libbroadcast is a C++ library for broadcasting video over RTMP to remote hosts. This library is a part of the [Mishira project](http://www.mishira.com).

License
=======

Unless otherwise specified all parts of Libbroadcast are licensed under the standard GNU General Public License v2 or later versions. A copy of this license is available in the file `LICENSE.GPL.txt`.

Any questions regarding the licensing of Libbroadcast can be sent to the authors via their provided contact details.

Version history
===============

A consolidated version history of Libbroadcast can be found in the `CHANGELOG.md` file.

Stability
=========

Libbroadcast's API is currently extremely unstable and can change at any time. The Libbroadcast developers will attempt to increment the version number whenever the API changes but users of the library should be warned that every commit to Libbroadcast could potentially change the API, ABI and overall behaviour of the library.

Building
========

Building Libbroadcast is nearly identical to building the main Mishira application. Detailed instructions for building Mishira can be found in the main Mishira Git repository. Right now development builds of Libbroadcast are compiled entirely within the main Visual Studio solution which is the `Libbroadcast.sln` file in the root of the repository. Please do not upgrade the solution or project files to later Visual Studio versions if asked.

Libbroadcast depends on Qt and Google Test. Instructions for building these dependencies can also be found in the main Mishira Git repository.

Contributing
============

Want to help out on Libbroadcast? We don't stop you! New contributors to the project are always welcome even if it's only a small bug fix or even if it's just helping spread the word of the project to others. You don't even need to ask; just do it!

More detailed guidelines on how to contribute to the project can be found in the main Mishira Git repository.
