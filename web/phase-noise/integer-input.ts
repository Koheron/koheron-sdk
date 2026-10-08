// PNA acquisition fields share integer editing; page metadata owns their limits.
function pnaIntegerInput(input: HTMLInputElement, value: number,
                         commit: (value: number) => Promise<number>, unitLabel = ''): NumberInput {
    const step = Number(input.step) || 1;
    return new NumberInput(input, {
        value, minimum: Number(input.min), maximum: Number(input.max),
        resolution: 1, integer: true, step, unitLabel,
        validate: next => {
            if (next % step !== 0) { throw new Error('Use an even decimation rate.'); }
        },
        commit
    });
}
