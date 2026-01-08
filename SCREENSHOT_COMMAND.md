# Screenshot Command Test

## Usage

The screenshot command can be used in two ways:

1. **With explicit client name:**
   ```
   screenshot client1
   ```

2. **With selected client (after using `choose`):**
   ```
   choose client1
   screenshot
   ```

## How it works

1. **Server side** (`ServerCommandController`):
   - Parses the `screenshot [client]` command
   - Sends a JSON request: `{"controller": "screenshot", "action": "take"}` to the client
   - Waits up to 10 seconds for response
   - Receives base64-encoded PNG data
   - Decodes and saves to `screenshot_<clientname>_<timestamp>.png` in current directory

2. **Client side** (`ClientScreenshotController`):
   - Receives the screenshot request
   - Tries screenshot tools in order: `scrot`, `import` (ImageMagick), `gnome-screenshot`
   - Takes screenshot and saves to `/tmp/rat_screenshot_<timestamp>.png`
   - Reads the file and encodes to base64
   - Sends back: `{"controller": "screenshot", "action": "take", "success": true, "data": "<base64>", "size": <bytes>}`
   - Cleans up temporary file

## Prerequisites

Client must have one of these installed:
- `scrot` (recommended, fastest)
- `import` (from ImageMagick package)
- `gnome-screenshot` (for GNOME desktop)

Install on Ubuntu/Debian:
```bash
sudo apt install scrot
# OR
sudo apt install imagemagick
# OR (usually pre-installed on GNOME)
sudo apt install gnome-screenshot
```

## Example Session

```
> list
Connected clients:
  - laptop_home (192.168.1.100:54321)
  - desktop_work (192.168.1.101:54322)

> screenshot laptop_home
Screenshot saved to screenshot_laptop_home_1704787200.png (1234567 bytes)

> choose desktop_work
Selected client: desktop_work

> screenshot
Screenshot saved to screenshot_desktop_work_1704787201.png (987654 bytes)
```

## Error Messages

- "No screenshot tool found" - Install scrot, ImageMagick, or gnome-screenshot
- "Timeout waiting for screenshot response" - Client took too long (>10 seconds)
- "No client specified and no client selected" - Use `choose <client>` or provide client name
- "Screenshot failed: ..." - Client-side error (check permissions, display)
