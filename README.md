Hexicord
=========
[![Latest Release](https://img.shields.io/github/release/foxcpp/Hexicord.svg?style=flat-squared)](https://github.com/foxcpp/Hexicord/releases/latest) [![Discord server](https://img.shields.io/discord/342774887091535873.svg?style=flat-square)](https://discord.gg/4Y6Xaf4) [![Issues](https://img.shields.io/github/issues-raw/foxcpp/Hexicord.svg?style=flat-square)](https://github.com/foxcpp/Hexicord/issues) [![License](https://img.shields.io/github/license/foxcpp/Hexicord.svg?style=flat-square")](https://github.com/foxcpp/Hexicord/blob/master/LICENSE)

[Discord API](https://discordapp.com/developers/docs/intro) implementation using C++11 with boost libraries.


Table of contents 
------------------
* [Features](#features)
* [Installation](#installation)
  * [Dependencies](#dependencies)
* [Usage](#usage)
* [Documentation](#documentation)
* [Contributing](#contributing)
* [License](#license)


### Features
* Gateway session with auto-reconnection on failure.
* Wrapper that hides weird API details.
* Using HTTP persistent connection to reduce overhead in series of requests.
* Minimal runtime dependencies.

### Installation

#### Requirements
* CMake 3.7+
* GCC 4.8.1+ or Clang 3.3 or other C++11 compatible compiler
* boost.asio (boost.system) 
* OpenSSL
* boost.beast (in-tree)
* nlohmann/json (in-tree)

Following command should be enough on debian-based distros:
```
# apt-get install cmake gcc libboost-system-dev libssl-dev
```


#### Installation
```
$ git clone https://github.com/foxcpp/Hexicord.git  # clone repository
$ cd Hexicord
$ git submodule update --init                       # download boost.beast
$ mkdir build; cd build
$ cmake .. -DCMAKE_BUILD_TYPE=Release               # configure build system, Release enabled optimizations.
```
You can also enable `HEXICORD_SHARED` if you need shared library, `HEXICORD_STATIC` is enabled by default.
```
$ make
```

### Usage

Main class of library is `Hexicord::Client`, which maintains gateway connection, dispatches events and provides interface to REST API.

There is simple echo bot:
```cpp
#include <boost/asio/io_service.hpp>
#include <hexicord/client.hpp>

using namespace Hexicord;

int main() {
    boost::asio::io_service ioService;
    Client client(ioService, "PUT YOUR TOKEN HERE");

    client.eventsDispatcher.addHandler(Event::MessageCreate, [&client](const nlohmann::json& message) {
        client.sendMessage(message["channel_id", message["content"]);
    });

    client.connectToGateway(client.getGatewayUrlBot()); // replace with client.getGatewayUrl() if not using bot account.
    client.run();
}
```

### Documentation

Documentation is generated using [Doxygen](http://www.stack.nl/~dimitri/doxygen/) if `HEXICORD_DOCS` option is enabled.

```
$ cmake . -DHEXICORD_DOCS=ON
$ make doc
```
Output will be in docs/.

Also documentation for latest release can be browsed online [here](https://foxcpp.github.io/Hexicord).


### Contributing

Here is some of the ways that you can contribute:
* Submit a bug report. We love hearing about broken things, so that we can fix them.
* Provide feedback. Even simple questions about how things work or why they were done a certain way carries value.
* Test code. Checkout `dev` branch, compile it and start trying to break stuff! :-)
* Write code. Check issues marked with [**help needed**](https://github.com/foxcpp/hexicord/issues?q=is%3Aissue+is%3Aopen+label%3A%22help+wanted%22) tag. 

##### Code style
In short: camelCase (snake_case for files), 4 spaces indent, { on same line.
Nobody likes to read long style guides, right? If you unsure about something else - just look at code.

### License 

Distributed under the MIT License.

```
Copyright (c) Maks Mazurov (fox.cpp) 2017 

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
