#!/usr/bin/python3
import getopt
import sys
import subprocess



def printHelp(name):
    print('Usage: %s [option] [args]' % name)
    print('     -h              #Usage')
    print('     -c              #Enable colored text')
    print('     -p PROG         #Program to test')
    print('     -t TESTCASE     #Testcase to test')
    print('     -v VLEVEL       #Set verbosity level (0-3)')
    print('     --valgrind      #Use valgrind')
    sys.exit(0)

def runTest(testCase, vLevel, isColored, useValgrind):
    testCaseDir = './testcases'
    RED = '\033[91m'
    GREEN = '\033[92m'
    WHITE = '\033[0m'
    color = WHITE
    command = ['./interpreter']
    testCases = ['testcase-01-ops.cmd',
                 'testcase-02-ops.cmd',
                 'testcase-03-ops.cmd',
                 'testcase-04-ops.cmd',
                 'testcase-05-ops.cmd',
                 'testcase-06-ops.cmd',
                 'testcase-07-robust.cmd',
                 'testcase-08-robust.cmd',
                 'testcase-09-perf.cmd',
                 'testcase-10-perf.cmd',
                 'testcase-11-perf.cmd',
                 'testcase-12-perf.cmd']
    
    scores = [6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6]

    if useValgrind:
        command = ['valgrind'] + command

    if testCase == 'allTestCases':
        tList = testCases
    elif not testCase in testCases:
        if isColored:
            color = RED
        print(color, '%s does not exist' % testCase, WHITE, sep='')
        color = WHITE
        return 
    else:
        tList = [testCase]
    print('---\tTest Case\t\tScore')
    totalScore = 0
    maxScore = 0
    i = 0
    for t in tList:
        fname = '%s/%s' % (testCaseDir, t)
        commandList = command +  ['-v', vLevel, '-f', fname]
        print('+++ Testing %s' % t)
        try:
            retcode = subprocess.call(commandList)
        except Exception as e:
            if isColored:
                color = RED
            print(color, "Call of '%s' failed: %s" % (" ".join(commandList), e), WHITE, sep='')
            color = WHITE

            return False
        getScore = 0
        if retcode == 0:
            getScore += scores[i]
            if isColored:
                color = GREEN
            print(color, '%s %d/%d' % (t, getScore, scores[i]), WHITE, sep='')
        else:
            if isColored:
                color = RED
            print(color, '%s %d/%d' % (t, getScore, scores[i]), WHITE, sep='')
        color = WHITE
        totalScore += getScore
        maxScore += scores[i]
        i += 1
        if totalScore < maxScore:
            if isColored:
                color = RED
            print(color, '---\tTotal\t\t%d/%d' % (totalScore,maxScore), WHITE, sep='')
        else:
            if isColored:
                color = GREEN
            print(color, '---\tTotal\t\t%d/%d' % (totalScore,maxScore), WHITE, sep='')
        color = WHITE



def run(name, args):
    prog = 'interpreter'
    testCase = 'allTestCases'
    isColored = False
    useValgrind = False
    vLevel = '1'

    optList, args = getopt.getopt(args, 'hcp:t:v:', ['valgrind'])

    for (opt, val) in optList:
        if opt == '-h':
            printHelp(name)
        elif opt == '-c':
            isColored = True
        elif opt == '-p':
            prog = val;
        elif opt == '-t':
            testCase = val
        elif opt == '-v':
            vLevel = val
        elif opt == '--valgrind':
            useValgrind = True
        else:
            print('unknown option %s', opt)
            printHelp(name)
    runTest(testCase, vLevel, isColored, useValgrind)
    

if __name__ == '__main__':
    run(sys.argv[0], sys.argv[1:])
