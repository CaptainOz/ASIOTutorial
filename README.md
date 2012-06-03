
ASIOTutorial
============

This is a series of simple tutorials to learn Boost ASIO.

Tutorial 1: Simple wget
-----------------------
This tutorial creates a simple application that connects to a URL provided on the command line,
downloads the page via HTTP synchronously, and prints it to `stdout`.


Tutorial 2: Synchronous HTTP Server
-----------------------------------
This tutorial creates a simple HTTP server that receives connections, parses HTTP GET requests, and
responds with a file. The files will be resolved relative to a directory passed in on the command
line when the server starts. It will also log the requests to `stdout`.

Tutorial 3: Asynchrnous wget
----------------------------
This tutorial creates a simple application that connects to a URL provided on the command line,
downloads the page via HTTP asynchronously, and prints it to `stdout`. The application will still be
single threaded however. This tutorial is similar to Tutorial 1: Simple wget, the main difference is
we will be using the `async_*` version of the Boost ASIO functions.

Tutorial 4: Asynchronous HTTP Server
------------------------------------
This tutorial creates a simple HTTP server that receives connections, parses HTTP GET requests, and
responds with a file. The files will be resolved relative to a directory passed in on the command
line when the server starts.

