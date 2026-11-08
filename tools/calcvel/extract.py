# Writes the given include: DeusExCalcVelocity's definition exactly as the
# given UActor_PhysWalking.cpp has it.
import sys
src = open(sys.argv[1], encoding='utf-8').read()
start = src.index('void UPawn::DeusExCalcVelocity(')
end = src.index('\n}\n', start) + 3
open(sys.argv[2], 'w', encoding='utf-8').write(src[start:end])
print(src[start:end].count('\n'), 'lines')
