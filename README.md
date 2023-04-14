# multiprocessing
Multiprocessing assignment for my Operating Systems class.

Multi-threaded driver (called processor) in C that will read image file
names from a folder. 


The processor’s parameters are <n:int>, <s:char>, <folder:char*> and <file
name: char*>. 
-- The <n> sets the maximum number of threads/child processes to exist at any
time.

-- s is a selector. If (s==’t’) the program will use the thread
solution and if (s==p) it will usee the process solution. 

-- The <folder:char*> parameter specifies the folder that contains the source images to be processed for the histogram extraction.

-- The <file name: char*> is the name of the output file to write the color
histograms to.
