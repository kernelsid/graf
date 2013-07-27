Help sheet for "graf"					S. Casner
=====================					9/11/2009

graf is a very lightweight graphing tool for use with the X Window
System.  The program was derived from the original X10 xgraph written
by David Harrison University of California, Berkeley 1986, 1987.  It
was "heavily hacked" by Van Jacobson and Steven McCanne, UCB/LBL to
add mouse functions to identify points and compute slopes and
distances, plus keystroke commands to toggle most of the visual
attributes of a window (grid, line, markers, etc.).  Additional
enhancements were added by Steve Casner at Packet Design.

graf is not intended for producing finished graphs for publication.
Rather, it is particularly useful for exploring data.  It allows
mulitple datasets to be compared by graphing them together as points,
lines or impulses, with easy switching among those modes and quick
hiding and unhiding each line as needed to see what is underneath.
You can display the value of any data point, measure the distance
between data points or calculate a slope, and zoom in on a subset of
the data for an expanded look.  The primary limitation is the lack of
a panning function.  You must create another zoomed view instead.


Data Format
-----------

The primary data format is just a list of X,Y coordinates entered as
two numbers separated by whitespace on a line.  Two additional numbers
can be added on each line to denote the width and height of error bars
associated with the data point.  Alternatively, if each line contains
a single number, then the number is interpreted as a Y value paired
with an X value that increments with each line.

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
be entered on the command line with multiple -N options.

Each dataset is shown in a different color with a different icon as
indicated by a legend listing the dataset labels in the right margin
of the graph.

If the input data begins with two lines, each of which begins with a
'#' character, then the first line is displayed as a title for the
graph, centered at the top of the window.  The graph title can
alternatively be entered with the -T option on the command line.


Command-line Options
--------------------

usage: graf [-B] [-c] [-e] [-m] [-M] [-n] [-o] [-p] [-r] [-R] [-s] [-S]
            [-t] [-u] [-x x1,x2] [-y y1,y2] [-l x] [-l y] [-N label]
            [-T title] [-d host:display] [-f font] [-g geometry] [
	    file ... ]

Fonts must be fixed width.
-B    Draw bar graph
-c    Don't use off-screen pixmaps to enhance screen redraws
-e    Draw error bars at each point
-l x  Logarithmic scale for X axis
-l y  Logarithmic scale for Y axis
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

Hold the left mouse button at any point int the space to see the
coordinates of that point.  If the X coordinate is a Unix timestamp
(seconds since 1970-01-01 00:00:00 GMT), then the coordinate will be
decoded into a GMT date and time.  If the mouse is on a data point,
then the dataset label, ordinal number of the data point, and any
optional comment text for that data point will also be displayed.
That datapoint identification string is also set as the X selection,
so you can paste it with the middle mouse button into another X window
such as an xterm.

Drag the left mouse button to show a triangle labeled with the delta
X, delta Y and slope values.  If the start and end points of the drag
are on data points, then the difference in the data point ordinal
numbers will also be shown.

Drag the middle mouse button to show the slope of the line between the
start and end points of the drag.

Drag the right mouse button to form the diagonal of a rectangle that
defines the boundaries of a zoomed view.  When you release the mouse
button, a new window will be created to contain that zoomed view.  You
can create as many zoomed views as you like, either to look at
different portions of the data or to zoom in multiple times to get the
the desired level.


Keystroke Commands
------------------

The following keyboard commands can be typed anywhere in the graf
window.  The are not case sensitive.

0 - 9    Toggles hiding of the Nth dataset
m        Toggles showing data point marks
p        If data point marks are shown, toggles between single pixel
         and data point icon
l        Toggles displaying lines between points
h        Toggles displaying an impulse for each data point (that is, a
         vertical line from the Y origin to the data point)
g or t   Toggles showing the grid vs just tick marks
b        Toggles showing bounding box
e        Toggles showing error bars (if dataset includes them)
r        Toggle drawing rectangles
f        Toggle drawing filled rectangles
s        Toggle drawing lines with splines
q        Closes the current window
Ctrl-D   Closes the current window
Ctrl-C   Quits the graf program
Ctrl-L   Redraw the window
