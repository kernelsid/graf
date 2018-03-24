Help sheet for "graf"
=====================

graf is a very lightweight graphing tool for use with the X Window
System.  The program was derived from the original X10 xgraph written
by David Harrison at University of California, Berkeley in 1986-1987.
It was "heavily hacked" by Van Jacobson and Steven McCanne at UCB/LBL
to add mouse functions to identify points and compute slopes and
distances, plus keystroke commands to toggle most of the visual
attributes of a window (grid, line, markers, etc.).  Additional
enhancements were added by Stephen Casner at Packet Design and later.

graf is not intended for producing finished graphs for publication.
Rather, it is particularly useful for exploring data.  It allows
multiple datasets to be compared by graphing them together as points,
lines or impulses, with easy switching among those modes and quick
hiding and unhiding each line as needed to see what is underneath.
You can display the value of any data point, measure the distance
between data points or calculate a slope or a linear regression, and
zoom in on a subset of the data for an expanded look.


Data Format
-----------

The primary data format is just a list of X,Y coordinates entered as
two numbers separated by whitespace on a line.  Two additional numbers
can be added on each line to denote the width and height of error bars
associated with the data point.  The four numbers can also be shown as
rectanges, open or filled, where the first two numbers X,Y determine
one corner of the rectangle and the third and fourth numbers give the
width and height of the rectangle up and to the right if positive or
down and left if negative.  This is useful for making bar graphs.

Alternatively, if each line contains a single number, then the number
is interpreted as a Y value paired with an X value that increments
with each line.

The data on each line can be followed by a '#' character and comment
text.  The comment text will be displayed when the left mouse button
is held down with the cursor on the corresponding data point.

graf can display multiple datasets entered either as multiple data
files on the command line, or as a single file with one blank line
between datasets.  The default label for each dataset is its filename
when one or more dataset files are entered on the command line, or
"Set 0", "Set 1", etc., if there are multiple datasets in one file.

If the first line of a dataset begins with '#', the text from that
line is taken as the label for the dataset.  Dataset labels can also
be entered on the command line with multiple -N options.  If the first
two or more lines of the input data begin with a '#' character, then
the first line is displayed as a title for the graph, centered at the
top of the window and the last of those lines is taken as the dataset
label.  The lines between the first and last are ignored as comments.
The graph title can alternatively be entered with the -T option on the
command line.

Each dataset is shown in a different color with a different icon as
indicated by a legend listing the dataset labels in the right margin
of the graph.


Command-line Options
--------------------

usage: graf [-B] [-c] [-D] [-L] [-e] [-m] [-M] [-n] [-o] [-p] [-r] [-R]
            [-s] [-S] [-t] [-u] [-x x1,x2] [-y y1,y2] [-l x] [-l y]
            [-N label] [-T title] [-d host:display] [-f font]
            [-g geometry] [ file ... ]

Fonts must be fixed width.

    -B    Draw bar graph
    -c    Don't use off-screen pixmaps to enhance screen redraws
    -D    Interpret X values as timestamps, label axis with date & time
    -e    Draw error bars at each point
    -f    Set font for mouse operation popups
    -l x  Logarithmic scale for X axis
    -l y  Logarithmic scale for Y axis
    -L    Show timestamps as local time not GMT
    -m    Mark each data point with large dot
    -M    Forces black and white
    -n    Don't draw lines (scatter plot)
    -N    Dataset label (repeat for multiple datasets)
    -o    Draws bounding box around data
    -p    If -m, mark each point with a pixel instead
    -r    Draw rectangle at each point (-R for filled)
    -s    X notion of spline curves
    -S    Set scale for slope display
    -t    Draw tick marks instead of full grid
    -T    Graph title
    -u    Input files are ulaw encoded


Mouse Functions
---------------

Hold the left mouse button at any point in the space to see the
coordinates of that point.  If the X coordinate value could be a Unix
timestamp (seconds since 1970-01-01 00:00:00 GMT) since 2000, then the
coordinate will be decoded into a date and time (GMT by default or
local with -L).  If the mouse is on a data point, then the dataset
label, ordinal number of the data point, and any optional comment text
for that data point will also be displayed.  The displayed string is
also set as the X11 primary selection, so you can paste it with the
middle mouse button into another X window such as an xterm.  If the
shift key and control key are held while the left mouse button is
clicked, then the presence of a data point at the cursor will be
ignored and just the coordinates will be shown.

Drag the left mouse button to show a triangle labeled with the delta
X, delta Y and slope values.  If the start and end points of the drag
are on data points, then the difference in the data point ordinal
numbers will also be shown.

Drag the middle mouse button to show the slope of the line between the
start and end points of the drag.  Hold the shift key and click the
middle mouse button on a data point to calculate and draw a
least-squares regression line for the data set containing that data
point.  While the button is held, the "y = mx + b" formula for the
line will be shown and set as the X11 primary selection.  The
regression line will be erased whenever the window is redrawn.

Drag the right mouse button to form the diagonal of a rectangle that
defines the boundaries of a zoomed view.  When you release the mouse
button, a new window will be created to contain that zoomed view.  You
can create as many zoomed views as you like, either to look at
different portions of the data or to zoom in multiple times to get the
the desired level.  Hold the shift key while dragging the right mouse
button to write out the subset of points spanned into the file
xgraph.tmp (there is no visual feedback after the file is written).

Scroll with a mouse wheel or trackpad gesture to pan the graph view
up, down, left or right.  If the X resource NaturalScroll is true the
direction is flipped so the graph is panned rather than the view.
Hold the shift key and scroll up to zoom in or down to zoom out at the
cursor.  Panning and zooming may be slow with many points in the view.


Keystroke Commands
------------------

The following keyboard commands are not case sensitive and can be
typed anywhere in the graf window.

    0 - 9    Toggles hiding of the Nth dataset; with Ctrl, N+10th
    E        Toggles showing error bars (if dataset includes them)
    F        Toggle drawing filled rectangles (for four-value data sets)
    G or T   Toggles showing the grid versus just tick marks
    H        Toggles drawing a vertical line from Y=0 to each data point
    L        Toggles displaying lines between points
    M        Toggles showing data point marks
    O        Toggles showing graph outline
    P        Toggles between single pixel and icon data point marks
    Q        Closes the current window
    R        Toggle drawing rectangles (for four-value data sets)
    S        Toggle drawing lines with splines (not implemented)
    ? or F1  Pops up a Help window listing mouse and keystroke functions
    Ctrl-C   Quits the graf program
    Ctrl-D   Closes the current window
    Ctrl-H   Pops up a Help window listing mouse and keystroke functions
    Ctrl-L   Redraw the window

When hiding of a dataset is toggled off, the dataset is redrawn on top
of the other datasets.  Ctrl-L will restore the original drawing order.


X Resources
-------------

graf will query the X server for resources defined under its name to
set defaults for the following parameters:

    Background     Background pixel color
    Border         Border color
    BorderSize     Border width in pixels
    ErrorBars      Draw error bars for four-value data sets
    Foreground     Foreground pixel color
    InfoFont       Font for popup messages (point ID, slope, etc.)
    LabelFont      Font for grid labels
    Markers        Draw a marker icon at each data point
    NaturalScroll  Pan the graph rather than the view when scrolling
    NoLines        Do not draw lines between data points
    Outline        Draw an outline around the graph area
    PixelMarkers   Mark data points with a single pixel, not an icon
    Rectangles     Draw rectangles for four-value data sets
    Text2Color     Color of second-level X-axis date labels
    Text3Color     Color of third-level X-axis date labels
    Ticks          Draw tick marks rather that full grid lines
    TitleFont      Font for graph title and Help window titles
    geometry       Specify the X11 geometry for the graf window
    precision      Number of digits of precision in popup messages

These resource parameters are typically set in a ~/.Xresources file,
but may also be set by other means.
