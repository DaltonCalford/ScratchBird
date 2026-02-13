# Appendix: Firebird 5 Status Vector Argument Tags (Authoritative)

These constants define the tags used in status vectors.

```
isc_arg_end = 0	/* end of argument list */
isc_arg_gds = 1	/* generic DSRI status value */
isc_arg_string = 2	/* string argument */
isc_arg_cstring = 3	/* count & string argument */
isc_arg_number = 4	/* numeric argument (long) */
isc_arg_interpreted = 5	/* interpreted status code (string) */
isc_arg_vms = 6	/* VAX/VMS status code (long) */
isc_arg_unix = 7	/* UNIX error code */
isc_arg_domain = 8	/* Apollo/Domain error code */
isc_arg_dos = 9	/* MSDOS/OS2 error code */
isc_arg_mpexl = 10	/* HP MPE/XL error code */
isc_arg_mpexl_ipc = 11	/* HP MPE/XL IPC error code */
isc_arg_next_mach = 15	/* NeXT/Mach error code */
isc_arg_netware = 16	/* NetWare error code */
isc_arg_win32 = 17	/* Win32 error code */
isc_arg_warning = 18	/* warning argument */
isc_arg_sql_state = 19	/* SQLSTATE */

```
