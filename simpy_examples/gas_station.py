"""
Simplified Gas Station example.
- Pumps represented as a Container with capacity 2 (two simultaneous users).
- Fuel tank is a Container with capacity 10, initially full.
- Two cars arrive, one every 5 time units. Each takes 8 units of fuel.
- Every 8 time units, a monitor checks the fuel tank; if level < 8, it schedules a tank truck to arrive in 3 units and refill the tank to full.
"""


import simpy

PUMP_CAPACITY = 2
FUEL_TANK_CAPACITY = 10
CAR_FUEL_NEED = 8
CAR_ARRIVAL_INTERVAL = 5
CHECK_INTERVAL = 8
REFUEL_TRUCK_DELAY = 3
LOW_THRESHOLD = 8
NUM_CARS = 2

MAX_TIME = 50


def car(name, env, pumps, fuel_tank):
    yield env.timeout((int(name.split()[-1]) * CAR_ARRIVAL_INTERVAL))  # stagger arrivals: Car 0 at 0, Car 1 at 5
    print(f"[{env.now}] {name} arrives at the gas station")
    # request a pump (decrement pump container)
    yield pumps.get(1)
    print(f"[{env.now}] {name} acquired a pump")
    # take fuel
    yield fuel_tank.get(CAR_FUEL_NEED)
    print(f"[{env.now}] {name} refueled with {CAR_FUEL_NEED} units")
    # release pump
    yield pumps.put(1)
    print(f"[{env.now}] {name} left the gas station")


def fuel_monitor(env, fuel_tank):
    while env.now <= MAX_TIME:
        yield env.timeout(CHECK_INTERVAL)
        if fuel_tank.level < LOW_THRESHOLD:
            print(f"[{env.now}] Fuel low (level={fuel_tank.level}), scheduling truck in {REFUEL_TRUCK_DELAY}")
            env.process(tank_truck(env, fuel_tank))
    return


def tank_truck(env, fuel_tank):
    yield env.timeout(REFUEL_TRUCK_DELAY)
    amount = fuel_tank.capacity - fuel_tank.level
    yield fuel_tank.put(amount)
    print(f"[{env.now}] Tank truck arrived and refilled station with {amount} units")


def main():
    env = simpy.Environment()
    pumps = simpy.Container(env, PUMP_CAPACITY, init=PUMP_CAPACITY)
    fuel_tank = simpy.Container(env, FUEL_TANK_CAPACITY, init=FUEL_TANK_CAPACITY)

    # start monitor
    env.process(fuel_monitor(env, fuel_tank))

    # spawn cars
    for i in range(NUM_CARS):
        env.process(car(f"Car {i}", env, pumps, fuel_tank))

    env.run(until=MAX_TIME)


if __name__ == "__main__":
    main()