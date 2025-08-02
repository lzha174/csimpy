"""
Deterministic carwash example mirroring the C++ version:
- Capacity 2 machines
- Service time 10 units
- Initial 4 cars arrive at time 0
- Then 5 additional cars arrive every 5 units
- FIFO queuing via simpy.Resource
"""
import simpy

# Parameters matching your C++ example
NUM_MACHINES = 2
WASHTIME = 10      # service time
EXTRA_CARS = 5
INTER_ARRIVAL = 5  # after initial batch

class Carwash:
    def __init__(self, env, num_machines, washtime):
        self.env = env
        self.machine = simpy.Resource(env, num_machines)
        self.washtime = washtime

    def wash(self, car_name):
        yield self.env.timeout(self.washtime)


def car(env, name, cw):
    print(f"[{env.now}] {name} arrives at the carwash.")
    with cw.machine.request() as request:
        yield request
        print(f"[{env.now}] {name} enters the carwash.")
        yield env.process(cw.wash(name))
        print(f"[{env.now}] {name} leaves the carwash.")


def setup(env, num_machines, washtime):
    cw = Carwash(env, num_machines, washtime)

    # Initial 4 cars at time 0
    for i in range(4):
        env.process(car(env, f"Car {i}", cw))

    # Then 5 additional cars arriving every INTER_ARRIVAL units
    for j in range(5):
        yield env.timeout(INTER_ARRIVAL)
        idx = 4 + j
        env.process(car(env, f"Car {idx}", cw))


def main():
    print("Carwash (deterministic C++-style version)")
    env = simpy.Environment()
    env.process(setup(env, NUM_MACHINES, WASHTIME))
    env.run()  # runs until all events are done

if __name__ == "__main__":
    main()