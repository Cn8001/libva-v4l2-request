# v4l2-request libVA Backend

## About

This libVA backend is designed to work with the Linux Video4Linux2
Request API that is used by a number of video codecs drivers,
including the Video Engine found in most Allwinner SoCs.

## Status

The v4l2-request libVA backend currently supports the following formats:
* MPEG2 (Simple and Main profiles)
* H264 (Baseline, Main and High profiles)
* H265 (Main profile)

## Instructions

In order to use this libVA backend, the `v4l2_request` driver has to
be specified through the `LIBVA_DRIVER_NAME` environment variable, as
such:

	export LIBVA_DRIVER_NAME=v4l2_request

A media player that supports VAAPI (such as VLC) can then be used to decode a
video in a supported format:

	vlc path/to/video.mpg

Sample media files can be obtained from:

	http://samplemedia.linaro.org/MPEG2/
	http://samplemedia.linaro.org/MPEG4/SVT/

## Technical Notes

### Surface

A Surface is an internal data structure never handled by the VA's user
containing the output of a rendering. Usualy, a bunch of surfaces are created
at the begining of decoding and they are then used alternatively. When
created, a surface is assigned a corresponding v4l capture buffer and it is
kept until the end of decoding. Syncing a surface waits for the v4l buffer to
be available and then dequeue it.

Note: since a Surface is kept private from the VA's user, it can ask to
directly render a Surface on screen in an X Drawable. Some kind of
implementation is available in PutSurface but this is only for development
purpose.

### Context

A Context is a global data structure used for rendering a video of a certain
format. When a context is created, input buffers are created and v4l's output
(which is the compressed data input queue, since capture is the real output)
format is set.

### Picture

A Picture is an encoded input frame made of several buffers. A single input
can contain slice data, headers and IQ matrix. Each Picture is assigned a
request ID when created and each corresponding buffer might be turned into a
v4l buffers or extended control when rendered. Finally they are submitted to
kernel space when reaching EndPicture.

The real rendering is done in EndPicture instead of RenderPicture
because the v4l2 driver expects to have the full corresponding
extended control when a buffer is queued and we don't know in which
order the different RenderPicture will be called.

### Image

An Image is a standard data structure containing rendered frames in a usable
pixel format. Here we only use NV12 buffers which are converted from sunxi's
proprietary tiled pixel format with tiled_yuv when deriving an Image from a
Surface.

---

# Kernel 6.x Port Addendum

The sections below document the state of this tree after the port to
the stable stateless codec API of kernel 6.x (tested against
linux-headers 6.18). The original notes above are kept unchanged.
See DEVLOG-kernel-6.18.md for the full story of the port.

## What the program does

The backend translates VA-API into the V4L2 Request API. The
application talks regular VA-API to libva, and this driver turns each
decode job into V4L2 stateless decoding requests: it fills the codec
control structures (SPS, PPS, slice and decode parameters), copies the
bitstream into V4L2 output buffers, ties everything together with a
media request and queues it to the kernel driver. The hardware decoder
writes the decoded frame into the capture buffer that backs the VA
surface.

Any V4L2 stateless decoder can be driven this way. Tested kernel
drivers are cedrus (Allwinner) and rpivid (Raspberry Pi 4/400 HEVC).
On the Pi, HEVC decoding has been verified bit-exact against software
decoding at 720p and 1080p. H264 and MPEG2 still compile and probe,
but the Pi has no stateless hardware for them.

Note for Raspberry Pi 4/400 users: only HEVC gets hardware decoding
through this backend on a Pi. The format list above describes what
the backend can translate, but it only works with stateless decoders
(the V4L2 Request API), because that is the model VA-API maps onto:
the application parses the bitstream and hands pre-parsed parameters
to the hardware for every frame. The only stateless decoder on the Pi
is rpivid (HEVC). The Pi's H264 decoder (bcm2835-codec) is stateful,
meaning the hardware parses the bitstream itself, which is a
different V4L2 interface this backend cannot drive. For H264 on the
Pi use the stateful decoder directly instead, for example FFmpeg's
h264_v4l2m2m decoder or mpv with --hwdec=v4l2m2m-copy. The full
MPEG2, H264 and H265 list applies to boards whose kernel driver
exposes all three as stateless decoders, such as Allwinner (cedrus).

Known limitations:
* Frames split into multiple slices only submit the parameters of the
  last slice, so multi-slice streams may show artifacts.
* HEVC scaling lists (IQ matrices) are not passed to the kernel yet.
  VA-API provides them in diagonal scan order while V4L2 expects
  raster order.
* Zero-copy export of SAND128 (Raspberry Pi) surfaces through DMA-BUF
  is untested. Use the copy path (vaGetImage / vaDeriveImage), which
  untiles in software and is known good.

## Building and installing with meson

The project needs libva and libdrm development files plus the kernel
uapi headers from a 6.x kernel:

	meson setup build
	ninja -C build
	sudo ninja -C build install

This installs `v4l2_request_drv_video.so` into the libva driver
directory of the chosen prefix, for example `/usr/local/lib/dri`.

## How to use

The driver is selected through environment variables. The first one
names the driver, the second one is only needed when the install
prefix is not on the default libva search path:

	export LIBVA_DRIVER_NAME=v4l2_request
	export LIBVA_DRIVERS_PATH=/usr/local/lib/dri

By default the backend opens `/dev/video0` and `/dev/media0`. Most
boards register the decoder elsewhere, so point it at the right nodes.
On a Raspberry Pi 4/400 the rpivid HEVC decoder is usually:

	export LIBVA_V4L2_REQUEST_VIDEO_PATH=/dev/video19
	export LIBVA_V4L2_REQUEST_MEDIA_PATH=/dev/media0

To find the right nodes on your board, check `v4l2-ctl --list-devices`
(from v4l-utils) or `dmesg | grep -i "registered as"`. The user also
needs permission to open the video and media nodes, which usually
means being in the `video` group:

	sudo usermod -aG video $USER

Then any VA-API capable player works:

	mpv --hwdec=vaapi-copy path/to/video.mkv
	ffplay -hwaccel vaapi path/to/video.mkv

## How to verify

Three levels of checking, from quick to thorough:

1. Driver loads and reports profiles:

		vainfo

	Success looks like `Driver version: v4l2-request` followed by the
	profile list (for example `VAProfileHEVCMain : VAEntrypointVLD`).
	A `vaInitialize failed` error almost always means the environment
	variables above are not set in the current shell.

2. Hardware actually decodes. Decode a file through the driver and
   watch for errors:

		ffmpeg -hwaccel vaapi -hwaccel_device /dev/dri/card0 \
			-i test.mkv -f null -

	The backend logs its V4L2 calls to stderr (s_fmt, create_bufs,
	streamon, dqbuf). If a frame fails in the kernel driver, the
	dequeue reports an error flag and the kernel log explains why,
	so also check `sudo dmesg`.

3. Output is correct. Compare hardware and software decodes frame by
   frame, the hashes must be identical:

		ffmpeg -hwaccel vaapi -i test.mkv -vf format=nv12 -f framemd5 hw.md5
		ffmpeg -i test.mkv -vf format=nv12 -f framemd5 sw.md5
		diff hw.md5 sw.md5 && echo "bit-exact"
