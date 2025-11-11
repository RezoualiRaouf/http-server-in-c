# Simple HTTP Server in C

A lightweight, multithreaded HTTP/1.1 server written in pure C with threads. For learning some network.

## Features

 **Multithreaded** - Handles concurrent connections using pthreads  
 **Directory serving** - Serve entire directory trees  
 **Single file mode** - Serve a specific file  
 **logs** - Request logs in standard format  
 **Security** - Path traversal protection  
 **MIME type detection** - Auto-detects content types (HTML, CSS, JS, images, etc.)

## Quick Start

### Compilation
```bash
gcc -pthread src/main.c src/netlib.c -I src -o server
```

### Usage Examples

**Serve a directory:**
```bash
./server -d ./public
```

**Serve a single file:**
```bash
./server -f index.html
```

**Custom port with logging:**
```bash
./server -d ./website -p 8080 -l access.log
```

**Display help:**
```bash
./server -h
```

## Command-Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `-d <directory>` | Serve files from directory | Current directory |
| `-f <file>` | Serve single file to all requests | - |
| `-p <port>` | Port number | 4221 |
| `-l <logfile>` | Log file path | stdout only |
| `-h` | Display help message | - |

## URL Structure

### Directory Mode (`-d`)
- `http://localhost:4221/` → serves `index.html`
- `http://localhost:4221/style.css` → serves `style.css`
- `http://localhost:4221/js/app.js` → serves `js/app.js`
- `http://localhost:4221/images/logo.png` → serves `images/logo.png`

### Single File Mode (`-f`)
```bash
./server -f mypage.html
```
- Must access: `http://localhost:4221/mypage.html` ✅
- Will 404: `http://localhost:4221/` ❌

## Log Format

The server logs requests in Apache Common Log Format:

```
127.0.0.1 - - [10/Nov/2025:14:32:20 +0100] "GET /index.html HTTP/1.1" 200 1234
```

## Project Structure

```
.
├── src/
│   ├── main.c          # Server initialization and main loop
│   ├── netlib.c        # HTTP handling and file serving
│   └── netlib.h        # Header file with declarations
├── server              # Compiled binary
└── README.md
```

## Security Features

-  Path traversal protection (`..` blocked)
-  Input validation

## Technical Details

- **Protocol**: HTTP/1.1
- **Concurrency**: One thread per connection (pthread)
- **Socket**: TCP (AF_INET, SOCK_STREAM)
- **Buffer Size**: 4KB request buffer
- **Response**: Supports Content-Type and Content-Length headers

## Testing

Start the server:
```bash
./server -d . -l server.log
```

Test with curl:
```bash
curl http://localhost:4221/README.md
curl -I http://localhost:4221/README.md  # Headers only
```

Test with browser (port 4221 is the default):
```
http://localhost:4221/
```

## Limitations

- Only GET method supported
- No HTTPS/TLS support
- No compression (gzip)
- No keep-alive connections
- No caching headers

## License

MIT License - Feel free to use and modify!

## Author

Built as a learning project to understand HTTP servers and network programming in C.

---

⭐ **Star this repo if you found it helpful!**
