#ifndef RECT_H
#define RECT_H

namespace av {

class Rect
{
public:
    Rect();
    Rect(int width, int height);
    Rect(int x, int y, int width, int height);


    void setX(int x) { this->x = x; }
    void setY(int y) { this->y = y; }
    void setWidth(int w) { width = w; }
    void setHeight(int h) { height = h; }

    int getX() { return x; }
    int getY() { return y; }
    int getWidth() { return width; }
    int getHeight() { return height; }

private:
    int x;
    int y;
    int width;
    int height;
};

} // ::av

#endif // RECT_H
