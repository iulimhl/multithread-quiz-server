# Quiz Master – Multithreaded Quiz Server

A multithreaded C/C++ server that coordinates a multiplayer trivia game with turn-based logic, score tracking, and socket communication.

## Features
- Unlimited clients connected via TCP sockets
- Turn-based gameplay with synchronized timing
- Time-limited questions with automatic scoring
- Server-only logic: clients only submit answers
- Can handle player disconnections
- Final scores and winner sent at end

## Demo
![Video](demo3.mp4)

## Technologies
- C/C++
- TCP Sockets
- POSIX Threads
- XML (for question storage)

## How to Run
1. Compile the server:
   gcc server.c -lpthread -o server
   ./server
2. In separate terminals, compile and start clients:
   gcc client.c -o client
   ./client
3. Each client will:
  -Connect to the server via TCP socket
  -Wait for their turn to answer
  -Receive a multiple-choice question (from SQLite or XML)
  -Send their selected answer within the allowed time (e.g., 10 seconds)
4. The server will:
  -Handle each client in a separate thread
  -Track scores and validate answers
  -Continue the quiz even if some clients disconnect
  -Announce the final score and winner when all questions are done

