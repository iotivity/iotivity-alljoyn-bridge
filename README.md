<!---
  ~ //******************************************************************
  ~ //
  ~ // Copyright 2016 Intel Corporation All Rights Reserved.
  ~ //
  ~ //-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  ~ //
  ~ // Licensed under the Apache License, Version 2.0 (the "License");
  ~ // you may not use this file except in compliance with the License.
  ~ // You may obtain a copy of the License at
  ~ //
  ~ //      http://www.apache.org/licenses/LICENSE-2.0
  ~ //
  ~ // Unless required by applicable law or agreed to in writing, software
  ~ // distributed under the License is distributed on an "AS IS" BASIS,
  ~ // WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  ~ // See the License for the specific language governing permissions and
  ~ // limitations under the License.
  ~ //
  ~ //-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  --->

## Install required libraries
Build IoTivity with the resource directory enabled and security disabled.

    $ scons RD_MODE=CLIENT,SERVER SECURED=0

Follow the instructions at
https://allseenalliance.org/framework/documentation/develop/building
for obtaining and building the required AllJoyn libraries.

## Build
To build you will need to copy the BuildOptions.txt.example to BuildOptions.txt
Open the files and update the change the build variables to reflect your
environment.

    $ cp BuildOptions.txt.example BuildOptions.txt
    $ vi BuildOptions.txt

To build run 'scons'. Variables specified in the BuildOptions.txt can be
overwriten by adding them to the scons command at runtime.

    $ scons BUILD_TYPE=release

## Testing
> **Linux Only** You will need to copy the setenv.sh.example to setenv.sh then setup
> the setenv.sh file to reflect your environment. You can then source the setenv.sh
> to setup the enviroment.
>
>     $ cp setenv.sh.example setenv.sh
>     $ vi setenv.sh
>     $ source setenv.sh

Currently the bridge requires multiple processes to manage multiple
instances of IoTivity (one per bridged AllJoyn device).  Under Linux a
helper application is provided to manage the processes.  The bridge
may be run as follows:

    $ ./out/linux/x86_64/debug/bin/PluginManager ./out/linux/x86_64/debug/bin/AllJoynBridge

Under Windows, run the AllJoynBridge process directly.  When the first
process outputs a line beginning with "exec ...", run a second
AllJoynBridge process with the args specified.  An example is below:

    $ .\out\windows\amd64\debug\bin\AllJoynBridge.exe
    ...
    exec --uuid b2120146-1c34-1f57-9ba7-ea7d817290fb --sender :svZOhUnr.2 --rd 37b48062-2533-d06a-8a1e-ce9e93739b09
    ...

    $ .\out\windows\amd64\debug\bin\AllJoynBridge.exe --uuid b2120146-1c34-1f57-9ba7-ea7d817290fb --sender :svZOhUnr.2 --rd 37b48062-2533-d06a-8a1e-ce9e93739b09
