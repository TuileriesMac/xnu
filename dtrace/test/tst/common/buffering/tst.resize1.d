/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)tst.resize1.d	1.1	06/08/28 SMI"

/*
 * ASSERTION:
 *	Checks that setting "bufresize" to "auto" will cause buffer
 *	allocation to succeed, even for large principal buffer sizes.
 *
 * SECTION: Buffers and Buffering/Buffer Resizing Policy;
 *	Options and Tunables/bufsize;
 *	Options and Tunables/bufresize
 *
 * NOTES:
 *	On some small memory machines, this test may consume so much memory
 *	that it induces memory allocation failure in the dtrace library.  This
 *	will manifest itself as an error like one of the following:
 *
 *	    dtrace: processing aborted: Memory allocation failure
 *	    dtrace: could not enable tracing: Memory allocation failure
 *
 *	These actually indicate that the test performed as expected; failures
 *	of the above nature should therefore be ignored.  If 32-bit libdtrace
 *      is used to execute this test on a 64-bit kernel, the failure mode
 *      becomes exceedingly likely because the kernel may be able to allocate
 *      a huge hunk of memory that then requires the client to match it with
 *      a malloc that exhausts the remainder of its 4GB address space.  For
 *      this reason, we no-op the test for 32-bit clients using an ifdef.
 */

#pragma D option bufresize=auto

BEGIN
{
	exit(0);
}
