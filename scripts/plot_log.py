import sys, os
import matplotlib.pyplot as plt
import subprocess

if __name__ == '__main__':
    
    if len(sys.argv) < 3:
        print('Usage:\n \t./plot_log.py <log_file> <ylim.max>')
        sys.exit()

    subprocess.call(['./parse_log.sh', sys.argv[1]])
    infile = open('aux.txt', 'r')

    iterations = []
    loss = []

    for line in infile:
        linestr = line.split(' ')
        loss.append(float(linestr[0]))
        iterations.append(int(linestr[1]))

    plt.ylim(ymax=float(sys.argv[2]), ymin=0)
    plt.grid(True)

    plt.xlabel('Iterations')
    plt.ylabel('Loss')
    plt.title('YOLO training loss curve')
    plt.plot(iterations, loss)
    # plt.show('yolo_loss.png')
    
    plt.savefig('yolo_loss.png')

    if open('aux.txt', 'r'):
        os.remove('aux.txt')
