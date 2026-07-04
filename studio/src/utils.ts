export function assertClass<T>(
	cls: abstract new (...args: readonly unknown[]) => T,
	instance: unknown,
): T {
	if (instance instanceof cls) {
		return instance
	}

	throw new Error(`"${instance}" is not of class "${cls.name}"`)
}
