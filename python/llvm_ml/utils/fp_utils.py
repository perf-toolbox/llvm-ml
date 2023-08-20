from decimal import Decimal

def floor_step(val, step_size: str):
    step_dec = Decimal(step_size)
    return float(int(Decimal(str(val)) / step_dec) * step_dec)
