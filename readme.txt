The self-tester is a tool that makes it easier to compare your implementation to the solution, helping you to identify bugs and to know what is considered correct behaviour.

How to run:
1. Copy your out/ directory into this directory.
2. Copy the files/ directory into your out/bin directory.
3. Run autorun.sh 

results: contains the dumps for each test case.
runs: populated with the images from running each test case.
solution-results: contains the dumps from running the solution on each test case.

Compare what's in solution-results with what's in results. You can use something like diff to help spot differences between the two; for example:

diff self-tester/results/case1-cp.img.txt self-tester/solution-results/case1-cp.img.txt

Notes:
- The inode reference mapping display ignores file size. It just scans the reference arrays up to their capacity.
- The concurrent tests produce a deterministic tree layout, but the creation of files itself is non-deterministic.
Therefore, a simple diff won't likely be sufficient to tell you if your result is correct, so you will have to 
inspect your resulting images as well.

Please report any problems or disagreements you encounter with using this tool or with the solution results
on piazza.

