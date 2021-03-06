/**
 * @file IxOsalOsSemaphore.c (linux)
 *
 * @brief Implementation for semaphore and mutex.
 * 
 * 
 * @par
 * IXP400 SW Release version 2.1
 * 
 * -- Copyright Notice --
 * 
 * @par
 * Copyright (c) 2001-2005, Intel Corporation.
 * All rights reserved.
 * 
 * @par
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * 
 * @par
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * 
 * @par
 * -- End of Copyright Notice --
 */

#include "IxOsal.h"

#include <linux/slab.h>
#ifdef IX_OSAL_OS_LINUX_VERSION_2_6
#include <linux/hardirq.h>
#else
#include <asm/hardirq.h>
#endif /* IX_OSAL_OS_LINUX_VERSION_2_6 */

/* Define a large number */
#define IX_OSAL_MAX_LONG (0x7FFFFFFF)

/* Max timeout in MS, used to guard against possible overflow */
#define IX_OSAL_MAX_TIMEOUT_MS (IX_OSAL_MAX_LONG/HZ)


PUBLIC IX_STATUS
ixOsalSemaphoreInit (IxOsalSemaphore * sid, UINT32 start_value)
{

    *sid = kmalloc (sizeof (struct semaphore), GFP_KERNEL);
    if (!(*sid))
    {
        ixOsalLog (IX_OSAL_LOG_LVL_ERROR,
            IX_OSAL_LOG_DEV_STDOUT,
            "ixOsalSemaphoreInit: fail to allocate for semaphore \n",
            0, 0, 0, 0, 0, 0);
        return IX_FAIL;
    }

    sema_init (*sid, start_value);

    return IX_SUCCESS;
}

/**
 * DESCRIPTION: If the semaphore is 'empty', the calling thread is blocked. 
 *              If the semaphore is 'full', it is taken and control is returned
 *              to the caller. If the time indicated in 'timeout' is reached, 
 *              the thread will unblock and return an error indication. If the
 *              timeout is set to 'IX_OSAL_WAIT_NONE', the thread will never block;
 *              if it is set to 'IX_OSAL_WAIT_FOREVER', the thread will block until
 *              the semaphore is available. 
 *
 *
 */


PUBLIC IX_STATUS
ixOsalSemaphoreWait (IxOsalOsSemaphore * sid, INT32 timeout)
{

    IX_STATUS ixStatus = IX_SUCCESS;
    unsigned long timeoutTime;

    if (sid == NULL)
    {
        ixOsalLog (IX_OSAL_LOG_LVL_ERROR,
            IX_OSAL_LOG_DEV_STDOUT,
            "ixOsalSemaphoreWait(): NULL semaphore handle \n",
            0, 0, 0, 0, 0, 0);
        return IX_FAIL;
    }

    /*
     * Guard against illegal timeout values 
     */
    if ((timeout < 0) && (timeout != IX_OSAL_WAIT_FOREVER))
    {
        ixOsalLog (IX_OSAL_LOG_LVL_ERROR,
            IX_OSAL_LOG_DEV_STDOUT,
            "ixOsalSemaphoreWait(): illegal timeout value \n",
            0, 0, 0, 0, 0, 0);
        return IX_FAIL;
    }

    if (timeout == IX_OSAL_WAIT_FOREVER)
    {
        down (*sid);
    }
    else if (timeout == IX_OSAL_WAIT_NONE)
    {
        if (down_trylock (*sid))
        {
            ixStatus = IX_FAIL;
        }
    }
    else if (timeout > IX_OSAL_MAX_TIMEOUT_MS)
    {
        ixOsalLog (IX_OSAL_LOG_LVL_ERROR,
            IX_OSAL_LOG_DEV_STDOUT,
            "ixOsalSemaphoreWait(): use a smaller timeout value to avoid overflow \n",
            0, 0, 0, 0, 0, 0);
        return IX_FAIL;
    }
    else
    {
        /* Convert timeout in milliseconds to HZ */
        timeoutTime = jiffies + (timeout * HZ) /1000 ;
        while (1)
        {
            if (!down_trylock (*sid))
            {
                break;
            }
            else
            {
                if (time_after(jiffies, timeoutTime))
                {
                    ixStatus = IX_FAIL;
                    break;
                }
            }
						/* Switch to next running process instantly */
            set_current_state(TASK_INTERRUPTIBLE);  
            schedule_timeout(1);

        }                       /* End of while loop */
    }                           /* End of if */

    return ixStatus;

}

/* 
 * Attempt to get semaphore, return immediately,
 * no error info because users expect some failures
 * when using this API.
 */
PUBLIC IX_STATUS
ixOsalSemaphoreTryWait (IxOsalSemaphore * sid)
{
    if (sid == NULL)
    {
        ixOsalLog (IX_OSAL_LOG_LVL_ERROR,
            IX_OSAL_LOG_DEV_STDOUT,
            "ixOsalSemaphoreTryWait(): NULL semaphore handle \n",
            0, 0, 0, 0, 0, 0);
        return IX_FAIL;
    }

    if (down_trylock (*sid))
    {
        return IX_FAIL;
    }

    return IX_SUCCESS;
}

/**
 *
 * DESCRIPTION: This function causes the next available thread in the pend queue
 *              to be unblocked. If no thread is pending on this semaphore, the 
 *              semaphore becomes 'full'. 
 */
PUBLIC IX_STATUS
ixOsalSemaphorePost (IxOsalSemaphore * sid)
{
    if (sid == NULL)
    {
        ixOsalLog (IX_OSAL_LOG_LVL_ERROR,
            IX_OSAL_LOG_DEV_STDOUT,
            "ixOsalSemaphorePost(): NULL semaphore handle \n",
            0, 0, 0, 0, 0, 0);
        return IX_FAIL;
    }

    up (*sid);
    return IX_SUCCESS;
}

PUBLIC IX_STATUS
ixOsalSemaphoreGetValue (IxOsalSemaphore * sid, UINT32 * value)
{
    ixOsalLog (IX_OSAL_LOG_LVL_MESSAGE,
        IX_OSAL_LOG_DEV_STDOUT,
        "ixOsalSemaphoreGetValue(): Not supported \n", 0, 0, 0, 0, 0, 0);
    return IX_SUCCESS;
}

PUBLIC IX_STATUS
ixOsalSemaphoreDestroy (IxOsalSemaphore * sid)
{
    if (sid == NULL)
    {
        ixOsalLog (IX_OSAL_LOG_LVL_ERROR,
            IX_OSAL_LOG_DEV_STDOUT,
            "ixOsalSemaphoreDestroy(): NULL semaphore handle \n",
            0, 0, 0, 0, 0, 0);
        return IX_FAIL;
    }

    kfree (*sid);
    return IX_SUCCESS;
}

/****************************
 *    Mutex 
 ****************************/

PUBLIC IX_STATUS
ixOsalMutexInit (IxOsalMutex * mutex)
{
    *mutex =
        (struct semaphore *) kmalloc (sizeof (struct semaphore), GFP_KERNEL);
    if (!mutex)
    {
        ixOsalLog (IX_OSAL_LOG_LVL_ERROR,
            IX_OSAL_LOG_DEV_STDOUT,
            "ixOsalMutexInit(): Fail to allocate for mutex \n",
            0, 0, 0, 0, 0, 0);
        return IX_FAIL;
    }

    init_MUTEX (*mutex);
    return IX_SUCCESS;
}

PUBLIC IX_STATUS
ixOsalMutexLock (IxOsalMutex * mutex, INT32 timeout)
{
    unsigned long timeoutTime;

    if (in_irq ())
    {
       return IX_FAIL;
    }

    if (mutex == NULL)
    {
        ixOsalLog (IX_OSAL_LOG_LVL_ERROR,
            IX_OSAL_LOG_DEV_STDOUT,
            "ixOsalMutexLock(): NULL mutex handle \n", 0, 0, 0, 0, 0, 0);
        return IX_FAIL;
    }

    if ((timeout < 0) && (timeout != IX_OSAL_WAIT_FOREVER))
    {
        ixOsalLog (IX_OSAL_LOG_LVL_ERROR,
            IX_OSAL_LOG_DEV_STDOUT,
            "ixOsalMutexLock(): Illegal timeout value \n", 0, 0, 0, 0, 0, 0);
        return IX_FAIL;
    }

    if (timeout == IX_OSAL_WAIT_FOREVER)
    {
        down (*mutex);
    }
    else if (timeout == IX_OSAL_WAIT_NONE)
    {
        if (down_trylock (*mutex))
        {
            return IX_FAIL;
        }
    }
    else if (timeout > IX_OSAL_MAX_TIMEOUT_MS)
    {
        ixOsalLog (IX_OSAL_LOG_LVL_ERROR,
            IX_OSAL_LOG_DEV_STDOUT,
            "ixOsalMutexLock(): use a smaller timeout value to avoid overflow \n",
            0, 0, 0, 0, 0, 0);
        return IX_FAIL;
    }
    else
    {
        timeoutTime = jiffies + (timeout * HZ) / 1000;

        while (1)
        {
            if (!down_trylock (*mutex))
            {
                break;
            }
            else
            {
                if (time_after(jiffies, timeoutTime))
                {
                    return IX_FAIL;
                }
            }

	    /* Switch to next running process if not in atomic state */
	    if (!in_atomic())
	    {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1);
	    }
        }                       /* End of while loop */
    }                           /* End of if */
    return IX_SUCCESS;
}

PUBLIC IX_STATUS
ixOsalMutexUnlock (IxOsalMutex * mutex)
{
    if (mutex == NULL)
    {
        ixOsalLog (IX_OSAL_LOG_LVL_ERROR,
            IX_OSAL_LOG_DEV_STDOUT,
            "ixOsalMutexUnlock(): NULL mutex handle \n", 0, 0, 0, 0, 0, 0);
        return IX_FAIL;
    }

    up (*mutex);
    return IX_SUCCESS;
}

/* 
 * Attempt to get mutex, return immediately,
 * no error info because users expect some failures
 * when using this API.
 */
PUBLIC IX_STATUS
ixOsalMutexTryLock (IxOsalMutex * mutex)
{
    if (mutex == NULL)
    {
        ixOsalLog (IX_OSAL_LOG_LVL_ERROR,
            IX_OSAL_LOG_DEV_STDOUT,
            "ixOsalMutexTryLock(): NULL mutex handle \n", 0, 0, 0, 0, 0, 0);
        return IX_FAIL;
    }

    if (down_trylock (*mutex))
    {
        return IX_FAIL;
    }

    return IX_SUCCESS;
}

PUBLIC IX_STATUS
ixOsalMutexDestroy (IxOsalMutex * mutex)
{
    if (mutex == NULL)
    {
        ixOsalLog (IX_OSAL_LOG_LVL_ERROR,
            IX_OSAL_LOG_DEV_STDOUT,
            "ixOsalMutexDestroy(): NULL mutex handle \n", 0, 0, 0, 0, 0, 0);
        return IX_FAIL;
    }
    kfree (*mutex);
    return IX_SUCCESS;
}

PUBLIC IX_STATUS
ixOsalFastMutexInit (IxOsalFastMutex * mutex)
{
    return mutex ? *mutex = 0, IX_SUCCESS : IX_FAIL;
}

PUBLIC IX_STATUS ixOsalFastMutexTryLock(IxOsalFastMutex *mutex)
{
    return ixOsalOemFastMutexTryLock(mutex);
}


PUBLIC IX_STATUS
ixOsalFastMutexUnlock (IxOsalFastMutex * mutex)
{
    if (mutex == NULL)
    {
        ixOsalLog (IX_OSAL_LOG_LVL_ERROR,
            IX_OSAL_LOG_DEV_STDOUT,
            "ixOsalFastMutexUnlock(): NULL mutex handle \n",
            0, 0, 0, 0, 0, 0);
        return IX_FAIL;
    }
    return mutex ? *mutex = 0, IX_SUCCESS : IX_FAIL;
}

PUBLIC IX_STATUS
ixOsalFastMutexDestroy (IxOsalFastMutex * mutex)
{
    if (mutex == NULL)
    {
        ixOsalLog (IX_OSAL_LOG_LVL_ERROR,
            IX_OSAL_LOG_DEV_STDOUT,
            "ixOsalFastMutexDestroy(): NULL mutex handle \n",
            0, 0, 0, 0, 0, 0);
        return IX_FAIL;
    }
    *mutex = 0;
    return IX_SUCCESS;
}
