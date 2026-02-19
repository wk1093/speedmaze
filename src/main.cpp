#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <chrono>
#include <unistd.h>
#include <locale.h>
#include <stdint.h>
#include <vector>
#include <queue>

FILE* _log_file;

#define LOG(str, ...) fprintf(_log_file, str ,##__VA_ARGS__); fflush(_log_file);

// A maze game where you adventure a randomly generated maze, the entire maze will be gray #, until the player can see the area, then the walls will be a white # and the empty spaces will be a space.
// the player will be a green @

// the maze will be a list of integers, 8 bit integers representing 8 tiles of the maze.
// the 8 tiles of an integer are always stacked horizontally, so the maze width must be a multiple of 8.

struct Maze {
    int* maze;
    int* explored;
    int width;
    int height;
    int* navmap=nullptr;
    int* dead=nullptr;
};

struct Player {
    int x;
    int y;
};


struct MazeGenRes {
    uint8_t** maze;
    int width;
    int height;
};

struct Camera {
    int xoffset;
    int yoffset;
};

void displayMaze(Maze maze, Player player, Camera* cam, Player nav={-1, -1}, bool checkexplore = true);

Maze convMazeNoEx(MazeGenRes res) {
    int width = (res.width+1) * 2;
    int height = (res.height+1) * 2;
    int* maze = new int[width * height / 8];

    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            int tile = 1;
            if (x % 2 == 1 && y % 2 == 1 && x < width - 1 && y < height - 1) {
                tile = 0;
            }
            
            maze[(y * width + x) / 8] = (maze[(y * width + x) / 8] & ~(1 << ((y * width + x) % 8))) | (tile << ((y * width + x) % 8));
        }
    }
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            if (x % 2 == 1 && y % 2 == 1 && x < width - 1 && y < height - 1) {
                uint8_t direction = res.maze[y / 2][x / 2];
                
                int newx = x + (direction == 1) - (direction == 3);
                int newy = y + (direction == 4) - (direction == 2);

                if (newx >= 0 && newx < width && newy >= 0 && newy < height) {
                    maze[(newy * width + newx) / 8] = (maze[(newy * width + newx) / 8] & ~(1 << ((newy * width + newx) % 8))) | (0 << ((newy * width + newx) % 8));
                }
            }
        }
    }

    return {maze, nullptr, width, height};
}

MazeGenRes mazeGen(int width, int height) {
    // origin shift algorithm
    // each cell is a direction
    uint8_t** maze = new uint8_t*[width];
    for (int i = 0; i < width; i++) {
        maze[i] = new uint8_t[height];
    }

    // every cell until the last column points right, then all the cells in the last column point down except the last one on the bottom which is the origin
    // 0 = origin, 1 = right, 2 = down, 3 = left, 4 = up

    Player origin = {width - 1, height - 1};

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (x == width - 1 && y == height - 1) {
                maze[y][x] = 0;
            } else if (x == width - 1) {
                maze[y][x] = 2;
            } else {
                maze[y][x] = 1;
            }
        }
    }

    Camera cam = {0, 0};

    int num_iters = 1000000;
    int num_display = 500;
    int disp_iter = num_iters / num_display;
    // pick a random direction to go from the origin (make sure we don't go out of bounds)
    for (int i = 0; i < num_iters; i++) {
        // int dir = rand() % 4;
        int dir = (rand() % 4) + 1;
        int newx = origin.x + (dir == 1) - (dir == 3);
        int newy = origin.y + (dir == 4) - (dir == 2);
        // bound check
        if (newx >= width || newx < 0 || newy >= height || newy < 0) {
            i--;
            continue;
        }
        
        maze[origin.y][origin.x] = dir;
        origin.x = newx;
        origin.y = newy;
        maze[origin.y][origin.x] = 0;

        if (i % disp_iter == 0) {
            Maze mazeobj = convMazeNoEx({maze, width, height});
            Player player = {origin.x * 2, origin.y * 2};
            displayMaze(mazeobj, player, &cam, {-1, -1}, false);
            refresh();
        }
    }

    return {maze, width, height};
}


Maze generateMaze(int width, int height) {
    if (width % 8 != 0) {
        printf("Width must be a multiple of 8, got %d\n", width);
        return {nullptr, 0, 0};
    }

    int* maze = new int[width * height / 8];
    int* explorered = new int[width * height / 8];

    for (int i = 0; i < width * height / 8; i++) {
        explorered[i] = 0;
    }

    MazeGenRes realMaze = mazeGen(width/2 - 1, height/2 - 1);

    // start with a full wall maze, then mepty each cell, and its respective neighbor from its direction.
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            int tile = 1;
            if (x % 2 == 1 && y % 2 == 1 && x < width - 1 && y < height - 1) {
                tile = 0;
            }
            
            maze[(y * width + x) / 8] = (maze[(y * width + x) / 8] & ~(1 << ((y * width + x) % 8))) | (tile << ((y * width + x) % 8));
        }
    }
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            if (x % 2 == 1 && y % 2 == 1 && x < width - 1 && y < height - 1) {
                uint8_t direction = realMaze.maze[y / 2][x / 2];
                
                int newx = x + (direction == 1) - (direction == 3);
                int newy = y + (direction == 4) - (direction == 2);

                if (newx >= 0 && newx < width && newy >= 0 && newy < height) {
                    maze[(newy * width + newx) / 8] = (maze[(newy * width + newx) / 8] & ~(1 << ((newy * width + newx) % 8))) | (0 << ((newy * width + newx) % 8));
                }
            }
        }
    }

    return {maze, explorered, width, height};
    
}


void displayMaze(Maze maze, Player player, Camera* cam, Player nav, bool checkexplore) {
    // keep the player in the center, unless the player is near the edge of the screen
    int scrwidth, scrheight;
    getmaxyx(stdscr, scrheight, scrwidth);

    scrwidth = scrwidth / 2;

    // if the player can be centered without displaying outside the maze, then center the player
    // if the width of the maze is less than the screen, the camera offset will always be 0 for x
    // same for height

    // if the player is near the edge, then the camera will be offset until the edge of the maze is on the edge of the screen then the offset will stop

    cam->xoffset = 0;
    cam->yoffset = 0;

    Player pl = player;
    if (nav.x != -1 && nav.y != -1) {
        pl = nav;
    }

    if (maze.width < scrwidth) {
        cam->xoffset = 0;
    } else {
        if (pl.x < scrwidth / 2) {
            cam->xoffset = 0;
        } else if (pl.x > maze.width - scrwidth / 2) {
            cam->xoffset = maze.width - scrwidth + 1;
        } else {
            cam->xoffset = pl.x - scrwidth / 2;
        }
    }

    if (maze.height < scrheight) {
        cam->yoffset = 0;
    } else {
        if (pl.y < scrheight / 2) {
            cam->yoffset = 0;
        } else if (pl.y > maze.height - scrheight / 2) {
            cam->yoffset = maze.height - scrheight + 1;
        } else {
            cam->yoffset = pl.y - scrheight / 2;
        }
    }


    // since characters are about double as tall as they are wide, we need to draw each tile twice horizontally

    for (int y = 0; y < scrheight; y++) {
        for (int x = 0; x < scrwidth; x++) {
            int realx = x + cam->xoffset;
            int realy = y + cam->yoffset;

            if (realx < 0 || realx >= maze.width-1 || realy < 0 || realy >= maze.height-1) {
                mvaddch(y, x * 2, ' ');
                mvaddch(y, x * 2 + 1, ' ');
                continue;
            }

            if (nav.x == realx && nav.y == realy) {
                attron(COLOR_PAIR(4));
                mvaddch(y, x * 2, '[');
                mvaddch(y, x * 2 + 1, ']');
                attroff(COLOR_PAIR(4));
                continue;
            }

            if (player.x == realx && player.y == realy) {
                attron(COLOR_PAIR(2));
                mvaddch(y, x * 2, '[');
                mvaddch(y, x * 2 + 1, ']');
                attroff(COLOR_PAIR(2));
                continue;
            }

            int tile = maze.maze[(realy * maze.width + realx) / 8];
            // int explored = maze.explored[(realy * maze.width + realx) / 8];
            int explored = 0;
            if (checkexplore) {
                explored = maze.explored[(realy * maze.width + realx) / 8];
            }
            int bit = (realy * maze.width + realx) % 8;

            if (!checkexplore || explored & (1 << bit)) {
                if (tile & (1 << bit)) {
                    attron(COLOR_PAIR(1));
                    mvaddch(y, x * 2, 'M');
                    mvaddch(y, x * 2 + 1, 'M');
                    attroff(COLOR_PAIR(1));
                } else {
                    // mvaddch(y, x * 2, ' ');
                    // mvaddch(y, x * 2 + 1, ' ');
                    if (maze.navmap != nullptr) {
                        int navtile = maze.navmap[(realy * maze.width + realx) / 8];
                        int navbit = (realy * maze.width + realx) % 8;
                        if (navtile & (1 << navbit)) {
                            attron(COLOR_PAIR(4));
                            mvaddch(y, x * 2, 'o');
                            mvaddch(y, x * 2 + 1, 'o');
                            attroff(COLOR_PAIR(4));
                        } else {
                            int deadtile = maze.dead[(realy * maze.width + realx) / 8];
                            int deadbit = (realy * maze.width + realx) % 8;
                            if (deadtile & (1 << deadbit)) {
                                attron(COLOR_PAIR(5));
                                mvaddch(y, x * 2, 'X');
                                mvaddch(y, x * 2 + 1, 'X');
                                attroff(COLOR_PAIR(5));
                            } else {
                                mvaddch(y, x * 2, ' ');
                                mvaddch(y, x * 2 + 1, ' ');
                            }
                        }
                    } else {
                        int deadtile = 0;
                        int deadbit = 0;
                        if (maze.dead != nullptr) {
                            deadtile = maze.dead[(realy * maze.width + realx) / 8];
                            deadbit = (realy * maze.width + realx) % 8;
                        }
                        
                        if (deadtile & (1 << deadbit)) {
                            attron(COLOR_PAIR(5));
                            mvaddch(y, x * 2, 'X');
                            mvaddch(y, x * 2 + 1, 'X');
                            attroff(COLOR_PAIR(5));
                        } else {
                            mvaddch(y, x * 2, ' ');
                            mvaddch(y, x * 2 + 1, ' ');
                        }
                    }
                }
            } else {
                attron(COLOR_PAIR(3));
                mvaddch(y, x * 2, '*');
                mvaddch(y, x * 2 + 1, '*');
                attroff(COLOR_PAIR(3));
            }
        }
    }
}



void exploreMaze(Maze maze, Player player, int depth = 0) {
    // raycast in all 4 directions from the player, until a wall is hit
    // mark all tiles included as explorered, including the hit wall
    // when we raycast we also want to do the tiles next to the ray. ex: casting right, we also want to mark the tile above and below the ray
    // this way we can see the walls around the player

    // 3x3 around player
    for (int y = player.y - 1; y <= player.y + 1; y++) {
        for (int x = player.x - 1; x <= player.x + 1; x++) {
            if (x < 0 || x >= maze.width || y < 0 || y >= maze.height) {
                continue;
            }
            maze.explored[(y * maze.width + x) / 8] |= 1 << ((y * maze.width + x) % 8);
        }
    }

    // right
    for (int x = player.x + 1; x < maze.width; x++) {
        // ray
        maze.explored[(player.y * maze.width + x) / 8] |= 1 << ((player.y * maze.width + x) % 8);
        // up/down
        if (player.y > 0) {
            maze.explored[((player.y - 1) * maze.width + x) / 8] |= 1 << (((player.y - 1) * maze.width + x) % 8);
        }
        if (player.y < maze.height - 1) {
            maze.explored[((player.y + 1) * maze.width + x) / 8] |= 1 << (((player.y + 1) * maze.width + x) % 8);
        }
        if (maze.maze[(player.y * maze.width + x) / 8] & (1 << ((player.y * maze.width + x) % 8))) {
            break;
        }
    }

    // left
    for (int x = player.x - 1; x >= 0; x--) {
        // ray
        maze.explored[(player.y * maze.width + x) / 8] |= 1 << ((player.y * maze.width + x) % 8);
        // up/down
        if (player.y > 0) {
            maze.explored[((player.y - 1) * maze.width + x) / 8] |= 1 << (((player.y - 1) * maze.width + x) % 8);
        }
        if (player.y < maze.height - 1) {
            maze.explored[((player.y + 1) * maze.width + x) / 8] |= 1 << (((player.y + 1) * maze.width + x) % 8);
        }
        if (maze.maze[(player.y * maze.width + x) / 8] & (1 << ((player.y * maze.width + x) % 8))) {
            break;
        }
    }

    // down
    for (int y = player.y + 1; y < maze.height; y++) {
        // ray
        maze.explored[(y * maze.width + player.x) / 8] |= 1 << ((y * maze.width + player.x) % 8);
        // left/right
        if (player.x > 0) {
            maze.explored[(y * maze.width + player.x - 1) / 8] |= 1 << ((y * maze.width + player.x - 1) % 8);
        }
        if (player.x < maze.width - 1) {
            maze.explored[(y * maze.width + player.x + 1) / 8] |= 1 << ((y * maze.width + player.x + 1) % 8);
        }
        if (maze.maze[(y * maze.width + player.x) / 8] & (1 << ((y * maze.width + player.x) % 8))) {
            break;
        }
    }

    // up
    for (int y = player.y - 1; y >= 0; y--) {
        // ray
        maze.explored[(y * maze.width + player.x) / 8] |= 1 << ((y * maze.width + player.x) % 8);
        // left/right
        if (player.x > 0) {
            maze.explored[(y * maze.width + player.x - 1) / 8] |= 1 << ((y * maze.width + player.x - 1) % 8);
        }
        if (player.x < maze.width - 1) {
            maze.explored[(y * maze.width + player.x + 1) / 8] |= 1 << ((y * maze.width + player.x + 1) % 8);
        }
        if (maze.maze[(y * maze.width + player.x) / 8] & (1 << ((y * maze.width + player.x) % 8))) {
            break;
        }
    }

    // if a dead cell is explored, then all connected dead cells should also be explored as well as surrounding cells
    // we can just call this same function again, but with the dead cell as the player
    if (depth > 1) return;
    if (maze.dead != nullptr) {
        for (int i = 0; i < maze.width * maze.height; i++) {
            if (maze.dead[i / 8] & (1 << (i % 8)) && maze.explored[i / 8] & (1 << (i % 8))) {
                Player deadplayer = {i % maze.width, i / maze.width};
                exploreMaze(maze, deadplayer, depth + 1); // probably should add a depth limit
            }
        }
    }
}

struct TileState {
    bool explored;
    bool wall;
};

TileState getTileState(Maze maze, int x, int y) {
    int tile = maze.maze[(y * maze.width + x) / 8];
    int bit = (y * maze.width + x) % 8;
    return {(maze.explored[(y * maze.width + x) / 8] & (1 << bit)) != 0, (tile & (1 << bit)) != 0};
}

TileState getPlayerTileState(Maze maze, Player player) {
    return getTileState(maze, player.x, player.y);
}

void navigateMaze(Maze* maze, Player from, Player to) {
    // we will do all the navigation calculations first, and then we will mark the path with the navmap
    // we will use a breadth first search to find the shortest path to the destination
    // we will use a queue to keep track of the tiles we need to explore

    // we will also keep track of the parent of each tile, so we can trace back the path

    if (maze->navmap != nullptr) {
        delete[] maze->navmap;
        maze->navmap = nullptr;
    }

    if (getTileState(*maze, to.x, to.y).wall) {
        return;
    }
    if (to.x == from.x && to.y == from.y) {
        return;
    }
    if (!getTileState(*maze, to.x, to.y).explored) {
        return;
    }


    std::queue<Player> q;
    q.push(from);

    int* parent = new int[maze->width * maze->height];

    for (int i = 0; i < maze->width * maze->height; i++) {
        parent[i] = -1;
    }

    parent[from.y * maze->width + from.x] = -2;

    while (!q.empty()) {
        Player current = q.front();
        q.pop();

        if (current.x == to.x && current.y == to.y) {
            break;
        }

        // right
        if (current.x < maze->width - 1) {
            if (!getTileState(*maze, current.x + 1, current.y).wall && parent[current.y * maze->width + current.x + 1] == -1) {
                q.push({current.x + 1, current.y});
                parent[current.y * maze->width + current.x + 1] = current.y * maze->width + current.x;
            }
        }

        // left
        if (current.x > 0) {
            if (!getTileState(*maze, current.x - 1, current.y).wall && parent[current.y * maze->width + current.x - 1] == -1) {
                q.push({current.x - 1, current.y});
                parent[current.y * maze->width + current.x - 1] = current.y * maze->width + current.x;
            }
        }

        // down
        if (current.y < maze->height - 1) {
            if (!getTileState(*maze, current.x, current.y + 1).wall && parent[(current.y + 1) * maze->width + current.x] == -1) {
                q.push({current.x, current.y + 1});
                parent[(current.y + 1) * maze->width + current.x] = current.y * maze->width + current.x;
            }
        }

        // up
        if (current.y > 0) {
            if (!getTileState(*maze, current.x, current.y - 1).wall && parent[(current.y - 1) * maze->width + current.x] == -1) {
                q.push({current.x, current.y - 1});
                parent[(current.y - 1) * maze->width + current.x] = current.y * maze->width + current.x;
            }
        }
    }

    // now that we have the parent of each tile, we can trace back the path

    Player current = to;

    maze->navmap = new int[maze->width * maze->height / 8];
    for (int i = 0; i < maze->width * maze->height / 8; i++) {
        maze->navmap[i] = 0;
    }

    while (parent[current.y * maze->width + current.x] != -2) {
        maze->navmap[(current.y * maze->width + current.x) / 8] |= 1 << ((current.y * maze->width + current.x) % 8);
        current = {parent[current.y * maze->width + current.x] % maze->width, parent[current.y * maze->width + current.x] / maze->width};
    }

    delete[] parent;
}



void deadAnalysis(Maze* maze, Player player) {
    // if a cell only has 2 directions, and one leads to an empty hallway (or other dead cells), then it is a dead cell
    if (maze->dead == nullptr) {
        maze->dead = new int[maze->width * maze->height / 8];
        for (int i = 0; i < maze->width * maze->height / 8; i++) {
            maze->dead[i] = 0;
        }
    }

    // for each cell, check if it is a dead cell
    for (int x = 0; x < maze->width; x++) {
        for (int y = 0; y < maze->height; y++) {
            if (getTileState(*maze, x, y).wall) {
                continue;
            }

            int numempty = 0;

            if (x < maze->width - 1 && !getTileState(*maze, x + 1, y).wall) numempty++;
            if (x > 0 && !getTileState(*maze, x - 1, y).wall) numempty++;
            if (y < maze->height - 1 && !getTileState(*maze, x, y + 1).wall) numempty++;
            if (y > 0 && !getTileState(*maze, x, y - 1).wall) numempty++;

            if (numempty == 1) {
                // this is a dead end
                maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
            }
            if (numempty == 2) {
                // if one direction leads to a dead end, then this is also a dead end
                if (x < maze->width - 1) {
                    if (!getTileState(*maze, x + 1, y).wall && (maze->dead[(y * maze->width + x + 1) / 8] & (1 << ((y * maze->width + x + 1) % 8)))) {
                        maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
                    }
                }
                if (x > 0) {
                    if (!getTileState(*maze, x - 1, y).wall && (maze->dead[(y * maze->width + x - 1) / 8] & (1 << ((y * maze->width + x - 1) % 8)))) {
                        maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
                    }
                }
                if (y < maze->height - 1) {
                    if (!getTileState(*maze, x, y + 1).wall && (maze->dead[((y + 1) * maze->width + x) / 8] & (1 << (((y + 1) * maze->width + x) % 8)))) {
                        maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
                    }
                }
                if (y > 0) {
                    if (!getTileState(*maze, x, y - 1).wall && (maze->dead[((y - 1) * maze->width + x) / 8] & (1 << (((y - 1) * maze->width + x) % 8)))) {
                        maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
                    }
                }
            }
        }
    }
    for (int x = maze->width - 1; x > 0; x--) {
        for (int y = 0; y < maze->height; y++) {
            if (getTileState(*maze, x, y).wall) {
                continue;
            }

            int numempty = 0;

            if (x < maze->width - 1 && !getTileState(*maze, x + 1, y).wall) numempty++;
            if (x > 0 && !getTileState(*maze, x - 1, y).wall) numempty++;
            if (y < maze->height - 1 && !getTileState(*maze, x, y + 1).wall) numempty++;
            if (y > 0 && !getTileState(*maze, x, y - 1).wall) numempty++;

            if (numempty == 1) {
                // this is a dead end
                maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
            }
            if (numempty == 2) {
                // if one direction leads to a dead end, then this is also a dead end
                if (x < maze->width - 1) {
                    if (!getTileState(*maze, x + 1, y).wall && (maze->dead[(y * maze->width + x + 1) / 8] & (1 << ((y * maze->width + x + 1) % 8)))) {
                        maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
                    }
                }
                if (x > 0) {
                    if (!getTileState(*maze, x - 1, y).wall && (maze->dead[(y * maze->width + x - 1) / 8] & (1 << ((y * maze->width + x - 1) % 8)))) {
                        maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
                    }
                }
                if (y < maze->height - 1) {
                    if (!getTileState(*maze, x, y + 1).wall && (maze->dead[((y + 1) * maze->width + x) / 8] & (1 << (((y + 1) * maze->width + x) % 8)))) {
                        maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
                    }
                }
                if (y > 0) {
                    if (!getTileState(*maze, x, y - 1).wall && (maze->dead[((y - 1) * maze->width + x) / 8] & (1 << (((y - 1) * maze->width + x) % 8)))) {
                        maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
                    }
                }
            }
        }
    }
    for (int x = 0; x < maze->width; x++) {
        for (int y = maze->height - 1; y > 0; y--) {
            if (getTileState(*maze, x, y).wall) {
                continue;
            }

            int numempty = 0;

            if (x < maze->width - 1 && !getTileState(*maze, x + 1, y).wall) numempty++;
            if (x > 0 && !getTileState(*maze, x - 1, y).wall) numempty++;
            if (y < maze->height - 1 && !getTileState(*maze, x, y + 1).wall) numempty++;
            if (y > 0 && !getTileState(*maze, x, y - 1).wall) numempty++;

            if (numempty == 1) {
                // this is a dead end
                maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
            }
            if (numempty == 2) {
                // if one direction leads to a dead end, then this is also a dead end
                if (x < maze->width - 1) {
                    if (!getTileState(*maze, x + 1, y).wall && (maze->dead[(y * maze->width + x + 1) / 8] & (1 << ((y * maze->width + x + 1) % 8)))) {
                        maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
                    }
                }
                if (x > 0) {
                    if (!getTileState(*maze, x - 1, y).wall && (maze->dead[(y * maze->width + x - 1) / 8] & (1 << ((y * maze->width + x - 1) % 8)))) {
                        maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
                    }
                }
                if (y < maze->height - 1) {
                    if (!getTileState(*maze, x, y + 1).wall && (maze->dead[((y + 1) * maze->width + x) / 8] & (1 << (((y + 1) * maze->width + x) % 8)))) {
                        maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
                    }
                }
                if (y > 0) {
                    if (!getTileState(*maze, x, y - 1).wall && (maze->dead[((y - 1) * maze->width + x) / 8] & (1 << (((y - 1) * maze->width + x) % 8)))) {
                        maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
                    }
                }
            }
        }
    }
    for (int x = maze->width-1; x > 0; x--) {
        for (int y = maze->height-1; y > 0; y--) {
            if (getTileState(*maze, x, y).wall) {
                continue;
            }

            int numempty = 0;

            if (x < maze->width - 1 && !getTileState(*maze, x + 1, y).wall) numempty++;
            if (x > 0 && !getTileState(*maze, x - 1, y).wall) numempty++;
            if (y < maze->height - 1 && !getTileState(*maze, x, y + 1).wall) numempty++;
            if (y > 0 && !getTileState(*maze, x, y - 1).wall) numempty++;

            if (numempty == 1) {
                // this is a dead end
                maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
            }
            if (numempty == 2) {
                // if one direction leads to a dead end, then this is also a dead end
                if (x < maze->width - 1) {
                    if (!getTileState(*maze, x + 1, y).wall && (maze->dead[(y * maze->width + x + 1) / 8] & (1 << ((y * maze->width + x + 1) % 8)))) {
                        maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
                    }
                }
                if (x > 0) {
                    if (!getTileState(*maze, x - 1, y).wall && (maze->dead[(y * maze->width + x - 1) / 8] & (1 << ((y * maze->width + x - 1) % 8)))) {
                        maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
                    }
                }
                if (y < maze->height - 1) {
                    if (!getTileState(*maze, x, y + 1).wall && (maze->dead[((y + 1) * maze->width + x) / 8] & (1 << (((y + 1) * maze->width + x) % 8)))) {
                        maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
                    }
                }
                if (y > 0) {
                    if (!getTileState(*maze, x, y - 1).wall && (maze->dead[((y - 1) * maze->width + x) / 8] & (1 << (((y - 1) * maze->width + x) % 8)))) {
                        maze->dead[(y * maze->width + x) / 8] |= 1 << ((y * maze->width + x) % 8);
                    }
                }
            }
        }
    }
}

int main() {
    setlocale(LC_ALL, "");
    _log_file = fopen("out.txt", "w+");

    LOG("STARTING\n");
    srand(time(NULL));
    initscr();
    noecho();
    cbreak();
    nodelay(stdscr, TRUE);
    curs_set(0);
    keypad(stdscr, TRUE);
    raw();
    start_color();
    init_color(COLOR_BLACK, 0, 0, 0);
    init_color(COLOR_WHITE, 1000, 1000, 1000);
    init_color(COLOR_GREEN, 0, 1000, 0);
    init_color(COLOR_MAGENTA, 300, 300, 1000);
    init_color(COLOR_RED, 1000, 0, 0);
    init_color(COLOR_BLUE, 200, 200, 200);
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(4, COLOR_RED, COLOR_BLACK);
    init_pair(5, COLOR_BLUE, COLOR_BLACK);
    bkgd(COLOR_PAIR(1));


    Player player = {1, 1};

    Player old_player = {0, 0};

    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    Maze maze = generateMaze(6*8, 6*8);
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    double millis = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000.0;
    LOG("Took %lf ms to generate maze\n", millis);

    Camera cam = {0, 0};

    float percentageexplored = 0;

    exploreMaze(maze, player);
    displayMaze(maze, player, &cam);
    refresh();

    bool navmode = false;

    // nav should dissapear after 2 seconds
    std::chrono::steady_clock::time_point startgame = std::chrono::steady_clock::now();

    std::chrono::steady_clock::time_point navstart = std::chrono::steady_clock::now();
    bool navdisplay = false;

    bool didwin = false;

    int ch;
    while ((ch = getch()) != 'q') {
        if (!navdisplay && maze.navmap != nullptr) {
            navdisplay = true;
            navstart = std::chrono::steady_clock::now();
        }
        if (navdisplay && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - navstart).count() > 500) {
            navdisplay = false;
            delete[] maze.navmap;
            maze.navmap = nullptr;
        }

        switch (ch) {
            case KEY_UP:
                if (player.y > 0) {
                    player.y--;
                }
                if (!navmode && getPlayerTileState(maze, player).wall) {
                    player.y++;
                }
                break;
            case KEY_DOWN:
                if (player.y < maze.height - 1) {
                    player.y++;
                }
                if (!navmode && getPlayerTileState(maze, player).wall) {
                    player.y--;
                }
                break;
            case KEY_LEFT:
                if (player.x > 0) {
                    player.x--;
                }
                if (!navmode && getPlayerTileState(maze, player).wall) {
                    player.x++;
                }
                break;
            case KEY_RIGHT:
                if (player.x < maze.width - 1) {
                    player.x++;
                }
                if (!navmode && getPlayerTileState(maze, player).wall) {
                    player.x--;
                }
                break;
            case 'n':
                navmode = !navmode;
                if (navmode) {
                    old_player = player;
                } else {
                    // now that we have selected a nav location, use an algorithm to find the shortest (only, since it is a perfect maze) path to the location
                    navigateMaze(&maze, old_player, player);
                    player = old_player;
                }
                break;
            case 'e':
            case 't':
                // if in navmode, we can press t instead of n, to teleport instead of navigate
                if (navmode) {
                    if (!getPlayerTileState(maze, player).wall && getPlayerTileState(maze, player).explored) {
                        navigateMaze(&maze, old_player, player);
                        old_player = player;
                    } else {
                        player = old_player;
                    }
                    navmode = false;
                } else {
                    navmode = true;
                    old_player = player;
                }
                break;
            case 'c':
                // explore everywhere instantly
                for (int i = 0; i < maze.width * maze.height / 8; i++) {
                    maze.explored[i] = 0xFF;
                }
                break;

            // WASD are same as arrow but move double the distance
            // we do need to do a bit more collision checking
            case 'w':
            case 'k':
                if (player.y > 1) {
                    player.y -= 2;
                }
                if (!navmode && (getPlayerTileState(maze, player).wall || getPlayerTileState(maze, {player.x, player.y + 1}).wall)) {
                    player.y += 2;
                }
                break;
            case 's':
            case 'j':
                if (player.y < maze.height - 2) {
                    player.y += 2;
                }
                if (!navmode && (getPlayerTileState(maze, player).wall || getPlayerTileState(maze, {player.x, player.y - 1}).wall)) {
                    player.y -= 2;
                }
                break;
            case 'a':
            case 'h':
                if (player.x > 1) {
                    player.x -= 2;
                }
                if (!navmode && (getPlayerTileState(maze, player).wall || getPlayerTileState(maze, {player.x + 1, player.y}).wall)) {
                    player.x += 2;
                }
                break;
            case 'd':
            case 'l':
                if (player.x < maze.width - 2) {
                    player.x += 2;
                }
                if (!navmode && (getPlayerTileState(maze, player).wall || getPlayerTileState(maze, {player.x - 1, player.y}).wall)) {
                    player.x -= 2;
                }
                break;
            

        }

        deadAnalysis(&maze, player);

        if (navmode) {
            displayMaze(maze, old_player, &cam, player);
        } else {
            exploreMaze(maze, player);
            displayMaze(maze, player, &cam);
        }
        percentageexplored = 0;
        for (int i = 0; i < maze.width * maze.height; i++) {
            if (maze.explored[i / 8] & (1 << (i % 8))) {
                percentageexplored++;
            }
        }
        percentageexplored = percentageexplored / ((maze.width-1) * (maze.height-1)) * 100;
        if (percentageexplored == 100) {
            didwin = true;
            break;
        }

        double precentagedead = 0;
        for (int i = 0; i < maze.width * maze.height; i++) {
            if (maze.dead[i / 8] & (1 << (i % 8))) {
                precentagedead++;
            }
        }

        precentagedead = precentagedead / ((maze.width-1) * (maze.height-1)) * 100;

        mvprintw(LINES - 2, 0, "Dead: %3.2f%%", precentagedead);

        // print Explored: %3.2f%% at the bottom of the screen
        mvprintw(LINES - 1, 0, "Explored: %3.2f%%", percentageexplored);

        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - startgame).count() / 1000000.0;
        mvprintw(LINES - 1, 20, "Time: %3.2f", elapsed);

        if (percentageexplored > 100) {
            // display to the user that this round is disqualified
            mvprintw(LINES - 1, 40, "DISQUALIFIED");
        }
        refresh();
    }
    std::chrono::steady_clock::time_point endgame = std::chrono::steady_clock::now();

    endwin();
    if (didwin) {
        printf("Took %lf\n", std::chrono::duration_cast<std::chrono::microseconds>(endgame - startgame).count() / 1000000.0);
        double cur = std::chrono::duration_cast<std::chrono::microseconds>(endgame - startgame).count() / 1000000.0;
        FILE* f = fopen("record.txt", "r");
        if (f == nullptr) {
            printf("New Record!\n");

        } else {
            double record;
            fscanf(f, "%lf", &record);
            fclose(f);
            if (cur < record) {
                printf("New Record!\n");
            } else {
                printf("Record: %lf\n", record);
                return 0;
            }
        }
        f = fopen("record.txt", "w");
        fprintf(f, "%lf", cur);
        fclose(f);
    }

    return 0;
}
