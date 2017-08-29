#******************************************************************
#
# Copyright 2016 Intel Corporation All Rights Reserved.
#
#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

import platform

vars = Variables('BuildOptions.txt')
vars.Add(EnumVariable('BUILD_TYPE', 'Specify release or debug build', 'debug', ['debug','release']))
vars.Add(PathVariable('ALLJOYN_DIST', 'Location of the alljoyn project', None, PathVariable.PathAccept))
vars.Add(PathVariable('IOTIVITY_BASE', 'Location of the iotivity project', None, PathVariable.PathAccept))
vars.Add(EnumVariable('IOTIVITY_LIB_TYPE', 'Specify release or debug build', 'release', ['debug','release']))
vars.Add(EnumVariable('TARGET_ARCH', 'Target architecture', 'x86_64', ['x86_64', 'amd64']))
vars.Add(BoolVariable('VERBOSE', 'Show compilation', False))
vars.Add(BoolVariable('COLOR', 'Enable color in build diagnostics, if supported by compiler', False))
vars.Add(EnumVariable('SECURED', 'Build with DTLS', '1', allowed_values=('0', '1')))
#vars.Add(EnumVariable('TEST', 'Run unit tests', '0', allowed_values=('0', '1')))
vars.Add(EnumVariable('MSVC_VERSION', 'MSVC compiler version - Windows', default=None, allowed_values=('12.0', '14.0')))
vars.Add(EnumVariable('MSVC_UWP_APP', 'Build a Universal Windows Platform (UWP) Application', default='0', allowed_values=('0', '1')))

env = Environment(variables = vars);
Help('''
Build options can be specified using the command line, BuildOptions.txt or using
the SCONS_FLAGS environment variable

Example of command line release build:

   $ scons BUILD_TYPE=release ALLJOYN_DIST=/path/to/ALLJOYN_DIST IOTIVITY_BASE=/path/to/IOTIVITY_BASE

The current options are:
''')
Help(vars.GenerateHelpText(env))

if env.get('VERBOSE') == False:
    env['CCCOMSTR'] = "Compiling $TARGET"
    env['SHCCCOMSTR'] = "Compiling $TARGET"
    env['CXXCOMSTR'] = "Compiling $TARGET"
    env['SHCXXCOMSTR'] = "Compiling $TARGET"
    env['LINKCOMSTR'] = "Linking $TARGET"
    env['SHLINKCOMSTR'] = "Linking $TARGET"
    env['ARCOMSTR'] = "Archiving $TARGET"
    env['RANLIBCOMSTR'] = "Indexing Archive $TARGET"

env['TARGET_OS'] = platform.system().lower()

# these must agree with options used in building IoTivity
env['CPPDEFINES'] = ['ROUTING_EP', 'RD_SERVER', 'RD_CLIENT']

if env['TARGET_OS'] == 'linux':

    env.Replace(BUILD_DIR = 'out/${TARGET_OS}/${TARGET_ARCH}/${BUILD_TYPE}' )

    # Set release/debug flags
    if env['BUILD_TYPE'] == 'release':
	env.AppendUnique(CCFLAGS = ['-Os'])
	env.AppendUnique(CPPDEFINES = ['NDEBUG'])
    else:
	env.AppendUnique(CCFLAGS = ['-g'])

    env.AppendUnique(CPPDEFINES = ['WITH_POSIX', '__linux__', 'QCC_OS_GROUP_POSIX'])
    env.AppendUnique(CFLAGS = ['-std=gnu99'])
    env.AppendUnique(CXXFLAGS = ['-std=c++11'])
    env.AppendUnique(CCFLAGS = ['-Werror', '-Wall', '-Wextra', '-Wno-ignored-qualifiers', '-Wno-maybe-uninitialized', '-fPIC'])

    target_arch = env.get('TARGET_ARCH')
    if target_arch in ['x86']:
	env.AppendUnique(CCFLAGS = ['-m32'])
	env.AppendUnique(LINKFLAGS = ['-m32'])
    elif target_arch in ['x86_64']:
	env.AppendUnique(CCFLAGS = ['-m64'])
	env.AppendUnique(LINKFLAGS = ['-m64'])

    env.AppendUnique(LIBS = ['pthread'])

elif env['TARGET_OS'] == 'windows':

    if env['MSVC_UWP_APP'] == '1':
        env.Replace(BUILD_DIR = 'out/${TARGET_OS}/uwp/${TARGET_ARCH}/${BUILD_TYPE}' )
    else:
        env.Replace(BUILD_DIR = 'out/${TARGET_OS}/win32/${TARGET_ARCH}/${BUILD_TYPE}' )

    # Set release/debug flags
    if env['BUILD_TYPE'] == 'release':
        env.AppendUnique(CCFLAGS = ['/MD', '/O2', '/GF'])
        env.AppendUnique(CPPDEFINES = ['NDEBUG'])
    else:
        env.AppendUnique(CCFLAGS = ['/MDd', '/Od', '/Zi', '/RTC1', '/Gm'])
        env.AppendUnique(LINKFLAGS = ['/debug'])

    env.AppendUnique(CXXFLAGS=[
        '/wd4244',   # C4244 conversion from one type to another type results in a possible loss of data.
        '/wd4267',   # C4267 conversion from size_t to a smaller type.
        '/wd4355',   # C4355 'this' used in base member initializer list.
        '/wd4800',   # C4800 forcing value to bool 'true' or 'false'.
        '/wd4996',   # C4996 deprecated declaration.
        '/wd4820',   # C4820 added padding to the end of a struct.
        '/wd4514',   # C4514 unreferenced inline function has been removed
        '/wd4365',   # C4365 signed/unsigned mismatch
        '/wd4503'])  # C4503 decorated name length exceeded, name was truncated

    env.AppendUnique(CPPDEFINES = ['QCC_OS_GROUP_WINDOWS'])
    env.AppendUnique(CCFLAGS=['/WX', '/EHsc'])
    env.AppendUnique(LIBS = ['bcrypt', 'crypt32', 'ws2_32', 'iphlpapi', 'shell32', 'ole32'])

    # Macro needed for Windows builds to avoid __declspec(dllexport) and __declspec(dllimport) for cJSON APIs.
    env.AppendUnique(CPPDEFINES = ['CJSON_HIDE_SYMBOLS'])

if env['SECURED'] == '1':
    env.AppendUnique(CPPDEFINES = ['__WITH_DTLS__=1'])
    env.AppendUnique(LIBS = ['mbedtls', 'mbedx509', 'mbedcrypto'])

if env.get('COLOR') == True:
    # If the gcc version is 4.9 or newer add the diagnostics-color flag
    # the adding diagnostics colors helps discover error quicker.
    gccVer = env['CCVERSION'].split('.')
    if int(gccVer[0]) > 4:
        env['CPPFLAGS'].append('-fdiagnostics-color');
    elif int(gccVer[0]) == 4 and int(gccVer[1]) >= 9:
        env['CPPFLAGS'].append('-fdiagnostics-color');

alljoyn_inc_paths = ['${ALLJOYN_DIST}/cpp/inc']
iotivity_resource_inc_paths = ['${IOTIVITY_BASE}/extlibs/tinycbor/tinycbor/src',
                               '${IOTIVITY_BASE}/extlibs/cjson',
	                       '${IOTIVITY_BASE}/resource/c_common/ocrandom/include',
	                       '${IOTIVITY_BASE}/resource/c_common/oic_malloc/include',
	                       '${IOTIVITY_BASE}/resource/c_common/oic_string/include',
                               '${IOTIVITY_BASE}/resource/c_common',
                               '${IOTIVITY_BASE}/resource/csdk/connectivity/api',
                               '${IOTIVITY_BASE}/resource/csdk/connectivity/lib/libcoap-4.1.1/include',
                               '${IOTIVITY_BASE}/resource/csdk/include',
                               '${IOTIVITY_BASE}/resource/csdk/logger/include',
                               '${IOTIVITY_BASE}/resource/csdk/resource-directory/include',
                               '${IOTIVITY_BASE}/resource/csdk/security/include',
                               '${IOTIVITY_BASE}/resource/csdk/security/provisioning/include',
                               '${IOTIVITY_BASE}/resource/csdk/security/provisioning/include/internal',
                               '${IOTIVITY_BASE}/resource/csdk/stack/include',
                               '${IOTIVITY_BASE}/resource/include',
                               '${IOTIVITY_BASE}/resource/oc_logger/include']

env['CPPPATH'] = ['${IOTIVITY_BASE}/extlibs']
env.AppendUnique(CPPPATH = alljoyn_inc_paths)
env.AppendUnique(CPPPATH = iotivity_resource_inc_paths)

env.AppendUnique(CPPPATH = ['#/include/'])

# libraries
env['LIBPATH'] = ['${ALLJOYN_DIST}/cpp/lib',
                  '${IOTIVITY_BASE}/extlibs/cjson']
if env['TARGET_OS'] == 'windows':
    env.AppendUnique(LIBPATH = '${IOTIVITY_BASE}/extlibs/sqlite3')
    if env['MSVC_UWP_APP'] == '1':
        env.AppendUnique(LIBPATH = '${IOTIVITY_BASE}/out/${TARGET_OS}/uwp/${TARGET_ARCH}/${IOTIVITY_LIB_TYPE}')
    else:
        env.AppendUnique(LIBPATH = '${IOTIVITY_BASE}/out/${TARGET_OS}/win32/${TARGET_ARCH}/${IOTIVITY_LIB_TYPE}')
else:
    env.AppendUnique(LIBPATH = '${IOTIVITY_BASE}/out/${TARGET_OS}/${TARGET_ARCH}/${IOTIVITY_LIB_TYPE}')

alljoynplugin_lib = env.SConscript('src/SConscript', variant_dir = env['BUILD_DIR'] + '/obj/src', exports= 'env', duplicate=0)
env.Install('${BUILD_DIR}/libs', alljoynplugin_lib)

# build examples
env.SConscript('examples/SConscript', variant_dir = env['BUILD_DIR'] + '/obj/examples', exports= ['env', 'iotivity_resource_inc_paths', 'alljoynplugin_lib'], duplicate=0)

# build tests
env.SConscript('test/SConscript', variant_dir = env['BUILD_DIR'] + '/obj/test', exports= ['env', 'iotivity_resource_inc_paths'], duplicate=0)

# build unit tests
env.SConscript('unittest/SConscript', variant_dir = env['BUILD_DIR'] + '/obj/unittests', exports=['env'], duplicate=0)
