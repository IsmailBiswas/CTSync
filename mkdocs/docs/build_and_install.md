# Clone repository
This project uses [Cequiq](https://github.com/IsmailBiswas/cequiq.git) as submodules so when
cloning this repository, use the `--recurse-submodules` flag:

```sh
git clone --recurse-submodules https://github.com/IsmailBiswas/CTSync.git
```
Otherwise, the server build will fail.

# Build and Run the Server

This section guides you through building and running the CTSync server. You
can choose one of three methods: building the Docker image locally, using a
pre-built image from GitHub Container Registry, or building and running it
directly on your host machine.

???+ failure "The server cannot be built and run directly on Windows or macOS."
    This server relies on Linux-specific APIs, primarily `epoll`, which is not
    available on Windows or macOS. As a result, it is not possible to build and
    run it natively on those platforms.

    However, on Windows, you can use WSL (Windows Subsystem for Linux) to build and run the server.
    On macOS, please consider using the docker option to run the server.

### Option 1: Build Locally Using Docker Compose

Ensure you have both `Docker` and `Docker Compose` installed on your system.

1.  **Navigate to the Docker Compose Directory:**

    ```bash
    cd backend/compose
    ```

2.  **Build the Docker Image:**

    ```bash
    docker compose build
    ```

    This command will build a Docker image named `ctsync_server` based on the `Dockerfile` in the current directory.

3.  **Generate Server Files:**

    Execute the `gen_server_files.sh` script located in the same directory:

    ```bash
    ./gen_server_files.sh
    ```

    This script will generate the following files within a newly created `server_files` directory:

    * A private key
    * A self-signed certificate
    * An example server join invite key file

4.  **Start the Server:**

    Use Docker Compose to start the server in detached mode:

    ```bash
    docker compose up -d
    ```

    By default, the server will expose port `4343` on your host machine.

5.  **Using a Different Port:**

    To use a different host port, set the `CTSYNC_PORT` environment variable before running the `docker compose up` command. For example, to use port `1111`:

    ```bash
    CTSYNC_PORT=1111 docker compose up -d
    ```
    Note that the log will show the server listening on the container's internal port, 4343. Your access, however, will be through the newly assigned port.

6.  **Initial Server Setup:**

    When you run the server for the first time, it will automatically create:

    * An SQLite database file within the `server_files` directory.
    * A file containing the SHA256 hash of the TLS certificate, also located in the `server_files` directory.

### Option 2: Use Pre-built Image from GitHub Container Registry

If you prefer not to build the image locally, you can use the pre-built image from GitHub Container Registry.

1.  **Pull the Docker Image:**

    ```bash
    docker pull ghcr.io/ismailbiswas/ctsync:latest
    ```

2.  **Generate Server Files (if you haven't already):**

    If you haven't generated the `server_files` directory using the script in
    Option 1, you'll need to do so now.

    ```bash
    ./gen_server_files.sh
    ```

3.  **Run the Server with Volume Mount:**

    Start the server, mounting your `server_files` directory to the container's expected location:

    ```bash
    docker run -d --name ctsync_server --restart=always -v ./server_files:/app/server_files -p 4343:4343 ghcr.io/ismailbiswas/ctsync:latest
    ```

    This command maps port `4343` on your host to port `4343` inside the container and mounts the `./server_files` directory into the `/app/server_files` directory within the container, making your generated TLS certificates and other server files available to the server.


???+ tip "Easy Setup with Default TLS"
    The server image includes default TLS certificate and key for immediate testing. To quickly run it:

    ```bash
    docker pull ghcr.io/ismailbiswas/ctsync:latest && docker run -d --name ctsync_server --restart=always -p 4343:4343 ghcr.io/ismailbiswas/ctsync:latest
    ```

    **Security WARNING:** The bundled TLS credentials are public and insecure. **For "production use", you MUST generate and mount your own `server_files`.**

### Option 3: Build and Run Directly on Your Host Machine

The server is a single-threaded C program that uses CMake as its build system.

**Build Requirements:**  
=== "Debian"
    ``` bash
    sudo apt update
    sudo apt install build-essential \
    cmake \
    libssl-dev \
    uuid-dev \
    uthash-dev \
    libsqlite3-dev \
    libcjson-dev
    ```

=== "Arch"
    ``` bash
    sudo pacman -Syu
    sudo pacman -S --needed \
    base-devel \
    uthash \
    cmake \
    openssl \
    sqlite \
    util-linux-libs \
    cjson
    ```

**Building the Server**  
After installing dependencies, navigate to the `/backend` directory and run:

```bash
mkdir -p build && cd build
cmake .. && make
```
This will generate the server binary inside the `build` directory.

### Server Required Files (Run Server)
The server needs **three files** to work properly. All three files must be present inside a directory named `server_files` at the current working directory. 

1. **Server Join Key File:**  
   This file must be named `invite_keys.txt`. Each line in this file should contain a alphanumeric string, followed by a space and an ISO 8601 format timestamp indicating the key's expiration time.  
   &nbsp;  
   When a new device wants to join the server, it must provide one of the keys specified in this file.  
   &nbsp;  
   Example invite_keys.txt file:
   ```
   invitekey123 2025-09-07T01:32:00Z #test invite key 1
   invitekey456 2025-07-07T01:32:00Z #test invite key 2
   ```
 

1. **Private Key:** This is the TLS private key file in PEM format. It must named `private_key.pem`
1. **Self Signed Certificate:** The self signed certificate file in PEM format. It must be named `certificate.pem`  

You can create all three of these files inside a `server_files` directory by running the `/backend/compose/gen_server_files.sh` script. 

Executing the server binary will automatically create a SQLite database
and a text file containing the server's certificate SHA256 hash within the
./server_files directory.


# Build Client

The client is a [Tauri](https://tauri.app/) application developed with [Vue.js](https://vuejs.org/) and the [shadcn-vue](https://www.shadcn-vue.com/) component library. Thanks to Tauri’s cross-platform framework, the client can be compiled for Linux, Windows, macOS, iOS, and Android.

### Prerequisites

Being a Tauri application, it requires Tauri’s specific [prerequisites](https://v2.tauri.app/start/prerequisites/) to be installed. While I've outlined them below, for the most accurate and current details, please refer to the [official Tauri documentation](https://v2.tauri.app/start/prerequisites/).


- [Install Rust](https://www.rust-lang.org/tools/install).  
- [Install Node.js](https://nodejs.org/en/download). 

Then install these system prerequistes.

=== "Debian"

    ``` bash
    sudo apt update
    sudo apt install libwebkit2gtk-4.1-dev \
    build-essential \
    curl \
    wget \
    file \
    libxdo-dev \
    libssl-dev \
    libayatana-appindicator3-dev \
    librsvg2-dev
    ```

=== "Arch"

    ``` bash
    sudo pacman -Syu
    sudo pacman -S --needed \
    webkit2gtk-4.1 \
    base-devel \
    curl \
    wget \
    file \
    openssl \
    appmenu-gtk-module \
    libappindicator-gtk3 \
    librsvg
    ```

=== "Windows"

    1. Download the [Microsoft C++ Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/) installer and open it to begin installation. During installation check the “Desktop development with C++” option.
    - [Install WebView2](https://v2.tauri.app/start/prerequisites/#webview2). (not required on Window10 and onward)


### Run Development Version
First, navigate to the **/frontend** directory and install npm packages.
```sh
npm install
```
Then to start the development version run:

```sh
npm run tauri dev
```

### Build Production Version
To build production version of the application run:

```sh
npm run tauri build
  
```

- On Linux, this generates .deb, .rpm, and an AppImage.
- On Windows, it creates .msi and .exe installers.


???+ failure "Arch build problem!"
    If you encounter a binary stripping problem when building the AppImage, a temporary fix is to set the `NO_STRIP=true` environment variable. This is an upstream problem.

## Build Client APK On Linux
???+ note "Android Restriction on Backgroup Cipboard Acesss"
    Heads up! Android 10 and newer versions are a bit strict about apps using
    the clipboard in the background. So, this app might not play nice on those
    versions. Weirdly though, it looks like pasting into the clipboard from a
    background app still works on Android 10 and up.

Ensure that the Android SDK and NDK are installed, and set the following environment variables:  

```sh
export ANDROID_HOME=/path/to/android/sdk  
export NDK_HOME=/path/to/android/ndk
```
The simplest way to get the SDK and NDK is to install [Android Studio](https://developer.android.com/studio) and use it to manage both.

After that, go to the `/frontend` directory and initialize an Android project by running:

```sh
npm run tauri android init
```

If you want, you can configure the icons to be used by running:

```sh
npm run tauri icon /path/to/icon.png

```

#### Android code signing
Next, you need to set up Android code signing; otherwise, you won't be able to
install the release version of the APK on a physical device.
The process involves first creating a `Keystore` file and then configuring
`gradle` to automatically use that `Keystore` file to sign the code whenever
an Android build is created. Please visit [this page↗️](https://tauri.app/distribute/sign/android/#configure-the-signing-key)
in the official Tauri documentation to set up Android code signing.  


#### Build and install
After setting up Android code signing, you can simply run:  

```sh
npm run tauri android build  
```
to build a signed APK.

If everything goes accordingly, at the end of the build, the APK file path will be shown. You can then use
```sh
adb install /path/to/apk/file
```
 to install it on a device or simply copy the APK to an Android device and install it manually.
