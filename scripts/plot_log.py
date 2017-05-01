import sys
import matplotlib.pyplot as plt

if __name__ == '__main__':
    
    if len(sys.argv) < 1:
        print('Please provide converted input log data.')
    else:
        infile = open(sys.argv[1], 'r')

    iterations = []
    loss = []

    for line in infile:
        linestr = line.split(' ')
        loss.append(float(linestr[0]))
        iterations.append(int(linestr[1]))

    plt.ylim(ymax=3, ymin=0)
    plt.grid(True)

    plt.xlabel('Iterations')
    plt.ylabel('Loss')
    plt.title('YOLO training loss curve')
    plt.plot(iterations, loss)
    # plt.show('yolo_loss.png')
    
    plt.savefig('yolo_loss.png')
