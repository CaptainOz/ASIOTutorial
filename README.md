
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


