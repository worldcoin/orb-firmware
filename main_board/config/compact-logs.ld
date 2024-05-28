log_fmt 0xF0000000 (INFO):
{
	/*
	Note: binutils >= 0.29 will automatically create this symbol but we set
	it explicitly for compatibility with older versions
	*/
	__start_log_fmt = ABSOLUTE(.);
	KEEP(*(*.log_fmt_hdr))
	KEEP(*(log_fmt))
}
