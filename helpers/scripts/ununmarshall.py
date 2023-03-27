import sys

def main():
    if len(sys.argv) != 2:
        print("Specify file")
        exit(0)

    with open(sys.argv[1], "r") as r, open(f"{sys.argv[1]}-clean", "w") as w:
        w.write("Requested_throughput,Achieved_throughput_sent,Achieved_throughput_received,Sent_size_received,Min,Avg,Max,Std-dev,Median\n")
        count = 1
        for line in r:
            if count < 10:
                count += 1
                continue

            if "[unmarshall]" in line or "test" in line or "Test" in line:
                continue
            w.write(line)

if __name__ == "__main__":
    main()
