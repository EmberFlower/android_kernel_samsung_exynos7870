/*
 * Copyright (c) 2014-2015 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*============================================================================
  FILE:         vos_lock.c

  OVERVIEW:     This source file contains definitions for vOS lock APIs
                The four APIs mentioned in this file are used for
                initializing , acquiring, releasing and destroying a lock.
                the lock are implemented using critical sections

  DEPENDENCIES:
============================================================================*/

/*============================================================================
  EDIT HISTORY FOR MODULE

============================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/

#include "vos_lock.h"
#include "vos_memory.h"
#include "vos_trace.h"
#include "vos_api.h"
#include "hif.h"
#include "i_vos_diag_core_event.h"
#ifdef CONFIG_CNSS
#include <net/cnss.h>
#endif
#include "vos_api.h"
#include "aniGlobal.h"

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
#define WIFI_POWER_EVENT_DEFAULT_WAKELOCK_TIMEOUT 0
#define WIFI_POWER_EVENT_WAKELOCK_TAKEN 0
#define WIFI_POWER_EVENT_WAKELOCK_RELEASED 1

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

#define LINUX_LOCK_COOKIE 0x12345678
enum
{
   LOCK_RELEASED = 0x11223344,
   LOCK_ACQUIRED,
   LOCK_DESTROYED
};

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
   Function Definitions and Documentation
 * -------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------

  \brief vos_lock_init() - initializes a vOSS lock

  The vos_lock_init() function initializes the specified lock. Upon
  successful initialization, the state of the lock becomes initialized
  and unlocked.

  A lock must be initialized by calling vos_lock_init() before it
  may be used in any other lock functions.

  Attempting to initialize an already initialized lock results in
  a failure.

  \param lock - pointer to the opaque lock object to initialize

  \return VOS_STATUS_SUCCESS - lock was successfully initialized and
          is ready to be used.

          VOS_STATUS_E_NOMEM - insufficient memory exists to initialize
          the lock

          VOS_STATUS_E_BUSY - The implementation has detected an attempt
          to reinitialize the object referenced by lock, a previously
          initialized, but not yet destroyed, lock.

          VOS_STATUS_E_FAULT  - lock is an invalid pointer.

          VOS_STATUS_E_FAILURE - default return value if it fails due to
          unknown reasons

       ***VOS_STATUS_E_RESOURCES - System resources (other than memory)
          are unavailable to initilize the lock
  \sa

    ( *** return value not considered yet )
  --------------------------------------------------------------------------*/
VOS_STATUS vos_lock_init ( vos_lock_t *lock )
{

   //check for invalid pointer
   if ( lock == NULL)
   {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: NULL pointer passed in",__func__);
       return VOS_STATUS_E_FAULT;
   }
   // check for 'already initialized' lock
   if ( LINUX_LOCK_COOKIE == lock->cookie )
   {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: already initialized lock",__func__);
       return VOS_STATUS_E_BUSY;
   }

   if (in_interrupt())
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s cannot be called from interrupt context!!!", __func__);
      return VOS_STATUS_E_FAULT;
   }

   // initialize new lock
   mutex_init( &lock->m_lock );
   lock->cookie = LINUX_LOCK_COOKIE;
   lock->state  = LOCK_RELEASED;
   lock->processID = 0;
   lock->refcount = 0;

   return VOS_STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------

  \brief vos_lock_acquire() - acquires a lock

  A lock object is acquired by calling \a vos_lock_acquire().  If the lock
  is already locked, the calling thread shall block   until the lock becomes
  available. This operation shall return with the lock object referenced by
  lock in the locked state with the calling thread as its owner.

  \param lock - the lock object to acquire

  \return VOS_STATUS_SUCCESS - the lock was successfully acquired by
          the calling thread.

          VOS_STATUS_E_INVAL - The value specified by lock does not refer
          to an initialized lock object.

          VOS_STATUS_E_FAULT  - lock is an invalid pointer.

          VOS_STATUS_E_FAILURE - default return value if it fails due to
          unknown reasons

  \sa
  ------------------------------------------------------------------------*/
VOS_STATUS vos_lock_acquire ( vos_lock_t* lock )
{
      int rc;
      //Check for invalid pointer
      if ( lock == NULL )
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: NULL pointer passed in",__func__);
         VOS_ASSERT(0);
         return VOS_STATUS_E_FAULT;
      }
      // check if lock refers to an initialized object
      if ( LINUX_LOCK_COOKIE != lock->cookie )
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: uninitialized lock",__func__);
         VOS_ASSERT(0);
         return VOS_STATUS_E_INVAL;
      }

      if (in_interrupt())
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s cannot be called from interrupt context!!!", __func__);
         VOS_ASSERT(0);
         return VOS_STATUS_E_FAULT;
      }
      if ((lock->processID == current->pid) &&
          (lock->state == LOCK_ACQUIRED))
      {
         lock->refcount++;
#ifdef VOS_NESTED_LOCK_DEBUG
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,"%s: %x %d %d", __func__, lock, current->pid, lock->refcount);
#endif
         return VOS_STATUS_SUCCESS;
      }
      // Acquire a Lock
      mutex_lock( &lock->m_lock );
      rc = mutex_is_locked( &lock->m_lock );
      if (rc == 0)
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: unable to lock mutex (rc = %d)", __func__, rc);
         VOS_ASSERT(0);
         return VOS_STATUS_E_FAILURE;
      }


#ifdef VOS_NESTED_LOCK_DEBUG
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,"%s: %x %d", __func__, lock, current->pid);
#endif
      if ( LOCK_DESTROYED != lock->state )
      {
         lock->processID = current->pid;
         lock->refcount++;
         lock->state    = LOCK_ACQUIRED;
         return VOS_STATUS_SUCCESS;
      }
      else
      {
         // lock is already destroyed
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: Lock is already destroyed", __func__);
         mutex_unlock(&lock->m_lock);
         VOS_ASSERT(0);
         return VOS_STATUS_E_FAILURE;
      }
}


/*--------------------------------------------------------------------------

  \brief vos_lock_release() - releases a lock

  The \a vos_lock_release() function shall release the lock object
  referenced by 'lock'.

  If a thread attempts to release a lock that it unlocked or is not
  initialized, an error is returned.

  \param lock - the lock to release

  \return VOS_STATUS_SUCCESS - the lock was successfully released

          VOS_STATUS_E_INVAL - The value specified by lock does not refer
          to an initialized lock object.

          VOS_STATUS_E_FAULT - The value specified by lock does not refer
          to an initialized lock object.

          VOS_STATUS_E_PERM - Operation is not permitted.  The calling
          thread does not own the lock.

          VOS_STATUS_E_FAILURE - default return value if it fails due to
          unknown reasons

  \sa
  ------------------------------------------------------------------------*/
VOS_STATUS vos_lock_release ( vos_lock_t *lock )
{
      //Check for invalid pointer
      if ( lock == NULL )
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: NULL pointer passed in",__func__);
         VOS_ASSERT(0);
         return VOS_STATUS_E_FAULT;
      }

      // check if lock refers to an uninitialized object
      if ( LINUX_LOCK_COOKIE != lock->cookie )
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: uninitialized lock",__func__);
         VOS_ASSERT(0);
         return VOS_STATUS_E_INVAL;
      }

      if (in_interrupt())
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s cannot be called from interrupt context!!!", __func__);
         VOS_ASSERT(0);
         return VOS_STATUS_E_FAULT;
      }

      // CurrentThread = GetCurrentThreadId();
      // Check thread ID of caller against thread ID
      // of the thread which acquire the lock
      if ( lock->processID != current->pid )
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: current task pid does not match original task pid!!",__func__);
#ifdef VOS_NESTED_LOCK_DEBUG
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,"%s: Lock held by=%d being released by=%d", __func__, lock->processID, current->pid);
#endif

         VOS_ASSERT(0);
         return VOS_STATUS_E_PERM;
      }
      if ((lock->processID == current->pid) &&
          (lock->state == LOCK_ACQUIRED))
      {
         if (lock->refcount > 0) lock->refcount--;
      }
#ifdef VOS_NESTED_LOCK_DEBUG
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,"%s: %x %d %d", __func__, lock, lock->processID, lock->refcount);
#endif
      if (lock->refcount) return VOS_STATUS_SUCCESS;

      lock->processID = 0;
      lock->refcount = 0;
      lock->state = LOCK_RELEASED;
      // Release a Lock
      mutex_unlock( &lock->m_lock );
#ifdef VOS_NESTED_LOCK_DEBUG
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,"%s: Freeing lock %x %d %d", lock, lock->processID, lock->refcount);
#endif
      return VOS_STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------

  \brief vos_lock_destroy() - Destroys a vOSS Lock - probably not required
  for Linux. It may not be required for the caller to destroy a lock after
  usage.

  The \a vos_lock_destroy() function shall destroy the lock object
  referenced by lock.  After a successful return from \a vos_lock_destroy()
  the lock object becomes, in effect, uninitialized.

  A destroyed lock object can be reinitialized using vos_lock_init();
  the results of otherwise referencing the object after it has been destroyed
  are undefined.  Calls to vOSS lock functions to manipulate the lock such
  as vos_lock_acquire() will fail if the lock is destroyed.  Therefore,
  don't use the lock after it has been destroyed until it has
  been re-initialized.

  \param lock - the lock object to be destroyed.

  \return VOS_STATUS_SUCCESS - lock was successfully destroyed.

          VOS_STATUS_E_BUSY - The implementation has detected an attempt
          to destroy the object referenced by lock while it is locked
          or still referenced.

          VOS_STATUS_E_INVAL - The value specified by lock is invalid.

          VOS_STATUS_E_FAULT  - lock is an invalid pointer.

          VOS_STATUS_E_FAILURE - default return value if it fails due to
          unknown reasons
  \sa
  ------------------------------------------------------------------------*/
VOS_STATUS vos_lock_destroy( vos_lock_t *lock )
{
      //Check for invalid pointer
      if ( NULL == lock )
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: NULL pointer passed in", __func__);
         return VOS_STATUS_E_FAULT;
      }

      if ( LINUX_LOCK_COOKIE != lock->cookie )
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: uninitialized lock", __func__);
         return VOS_STATUS_E_INVAL;
      }

      if (in_interrupt())
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s cannot be called from interrupt context!!!", __func__);
         return VOS_STATUS_E_FAULT;
      }

      // check if lock is released
      if (!mutex_trylock(&lock->m_lock))
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: lock is not released", __func__);
         return VOS_STATUS_E_BUSY;
      }
      lock->cookie = 0;
      lock->state = LOCK_DESTROYED;
      lock->processID = 0;
      lock->refcount = 0;

      mutex_unlock(&lock->m_lock);


      return VOS_STATUS_SUCCESS;
}


/*--------------------------------------------------------------------------

  \brief vos_spin_lock_init() - initializes a vOSS spin lock

  The vos_spin_lock_init() function initializes the specified spin lock. Upon
  successful initialization, the state of the lock becomes initialized
  and unlocked.

  A lock must be initialized by calling vos_spin_lock_init() before it
  may be used in any other lock functions.

  Attempting to initialize an already initialized lock results in
  a failure.

  \param pLock - pointer to the opaque lock object to initialize

  \return VOS_STATUS_SUCCESS - spin lock was successfully initialized and
          is ready to be used.
  --------------------------------------------------------------------------*/

VOS_STATUS vos_spin_lock_init(vos_spin_lock_t *pLock)
{
   spin_lock_init(pLock);

   return VOS_STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------

  \brief vos_spin_lock_acquire() - acquires a spin lock

  A lock object is acquired by calling \a vos_spin_lock_acquire().  If the lock
  is already locked, the calling thread shall spin until the lock becomes
  available. This operation shall return with the lock object referenced by
  lock in the locked state with the calling thread as its owner.

  \param pLock - the lock object to acquire

  \return VOS_STATUS_SUCCESS - the lock was successfully acquired by
          the calling thread.

  \sa
  ------------------------------------------------------------------------*/
VOS_STATUS vos_spin_lock_acquire(vos_spin_lock_t *pLock)
{
   spin_lock(pLock);
   return VOS_STATUS_SUCCESS;
}
/*--------------------------------------------------------------------------

  \brief vos_spin_lock_release() - releases a lock

  The \a vos_lock_release() function shall release the spin lock object
  referenced by 'lock'.

  If a thread attempts to release a lock that it unlocked or is not
  initialized, an error is returned.

  \param pLock - the lock to release

  \return VOS_STATUS_SUCCESS - the lock was successfully released

  \sa
  ------------------------------------------------------------------------*/
VOS_STATUS vos_spin_lock_release(vos_spin_lock_t *pLock)
{
   spin_unlock(pLock);
   return VOS_STATUS_SUCCESS;
}


/*--------------------------------------------------------------------------

  \brief vos_spin_lock_destroy() - releases resource of a lock

  \param pLock - the pointer to a lock to release

  \return VOS_STATUS_SUCCESS - the lock was successfully released

  \sa
  ------------------------------------------------------------------------*/
VOS_STATUS vos_spin_lock_destroy(vos_spin_lock_t *pLock)
{

   return VOS_STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------

  \brief vos_wake_lock_init() - initializes a vOSS wake lock

  \param pLock - the wake lock to initialize
              name - wakelock name

  \return VOS_STATUS_SUCCESS - wake lock was successfully initialized and
          is ready to be used.
  --------------------------------------------------------------------------*/
VOS_STATUS vos_wake_lock_init(vos_wake_lock_t *pLock, const char *name)
{
#if defined CONFIG_CNSS
    cnss_pm_wake_lock_init(&pLock->lock, name);
#elif defined(WLAN_OPEN_SOURCE) && defined(CONFIG_HAS_WAKELOCK)
    wake_lock_init(&pLock->lock, WAKE_LOCK_SUSPEND, name);
#endif
    pLock->runtime_pm_context = vos_runtime_pm_prevent_suspend_init(name);

    return VOS_STATUS_SUCCESS;
}

/**
 * vos_wake_lock_name() - This function returns the name of the wakelock
 * @pLock: Pointer to the wakelock
 *
 * This function returns the name of the wakelock
 *
 * Return: Pointer to the name if it is valid or a default string
 *
 */
static const char* vos_wake_lock_name(vos_wake_lock_t *pLock)
{
#if defined CONFIG_CNSS
	if (pLock->lock.name)
		return pLock->lock.name;
#elif defined(WLAN_OPEN_SOURCE) && defined(CONFIG_HAS_WAKELOCK)
	if (pLock->lock.ws.name)
		return pLock->lock.ws.name;
#endif
	return "UNNAMED_WAKELOCK";
}

/*--------------------------------------------------------------------------

  \brief vos_wake_lock_acquire() - acquires a wake lock

  \param pLock - the wake lock to acquire

  \return VOS_STATUS_SUCCESS - the wake lock was successfully acquired

  ------------------------------------------------------------------------*/
VOS_STATUS vos_wake_lock_acquire(vos_wake_lock_t *pLock,
                                 uint32_t reason)
{
    vos_log_wlock_diag(reason, vos_wake_lock_name(pLock),
                       WIFI_POWER_EVENT_DEFAULT_WAKELOCK_TIMEOUT,
                       WIFI_POWER_EVENT_WAKELOCK_TAKEN);

    /*
     * Dont prevent Autosuspend for these reasons, either it is not required to
     * do so or runtime functionality is not available at this time
     */
    switch(reason) {
    case WIFI_POWER_EVENT_WAKELOCK_DRIVER_INIT:
    case WIFI_POWER_EVENT_WAKELOCK_DRIVER_REINIT:
        break;
    default:
        vos_runtime_pm_prevent_suspend(pLock->runtime_pm_context);
        break;
    }
#if defined CONFIG_CNSS
    cnss_pm_wake_lock(&pLock->lock);
#elif defined(WLAN_OPEN_SOURCE) && defined(CONFIG_HAS_WAKELOCK)
    wake_lock(&pLock->lock);
#elif defined(CONFIG_NON_QC_PLATFORM)
#if defined(QCA_WIFI_2_0) && !defined(QCA_WIFI_ISOC)
    vos_runtime_pm_prevent_suspend(pLock->runtime_pm_context);
#endif
#endif
    return VOS_STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------

  \brief vos_wake_lock_timeout_acquire() - acquires a wake lock with a timeout

  \param pLock - the wake lock to acquire

  \return VOS_STATUS_SUCCESS - the wake lock was successfully acquired

  ------------------------------------------------------------------------*/
VOS_STATUS vos_wake_lock_timeout_acquire(vos_wake_lock_t *pLock, v_U32_t msec,
                                         uint32_t reason)
{
    /* Wakelock for Rx is frequent.
     * It is reported only during active debug
     */
    if (((vos_get_ring_log_level(RING_ID_WAKELOCK) >= WLAN_LOG_LEVEL_ACTIVE)
         && (WIFI_POWER_EVENT_WAKELOCK_HOLD_RX == reason)) ||
         (WIFI_POWER_EVENT_WAKELOCK_HOLD_RX != reason)) {
        vos_log_wlock_diag(reason, vos_wake_lock_name(pLock), msec,
                           WIFI_POWER_EVENT_WAKELOCK_TAKEN);
    }

    vos_runtime_pm_prevent_suspend_timeout(pLock->runtime_pm_context, msec);
#if defined CONFIG_CNSS
    cnss_pm_wake_lock_timeout(&pLock->lock, msec);
#elif defined(WLAN_OPEN_SOURCE) && defined(CONFIG_HAS_WAKELOCK)
    wake_lock_timeout(&pLock->lock, msecs_to_jiffies(msec));
#endif
    return VOS_STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------

  \brief vos_wake_lock_release() - releases a wake lock

  \param pLock - the wake lock to release

  \return VOS_STATUS_SUCCESS - the lock was successfully released

  ------------------------------------------------------------------------*/
VOS_STATUS vos_wake_lock_release(vos_wake_lock_t *pLock, uint32_t reason)
{
    vos_log_wlock_diag(reason, vos_wake_lock_name(pLock),
                       WIFI_POWER_EVENT_DEFAULT_WAKELOCK_TIMEOUT,
                       WIFI_POWER_EVENT_WAKELOCK_RELEASED);
#if defined CONFIG_CNSS
    cnss_pm_wake_lock_release(&pLock->lock);
#elif defined(WLAN_OPEN_SOURCE) && defined(CONFIG_HAS_WAKELOCK)
    wake_unlock(&pLock->lock);
#elif defined(CONFIG_NON_QC_PLATFORM)
#if defined(QCA_WIFI_2_0) && !defined(QCA_WIFI_ISOC)
    vos_runtime_pm_allow_suspend(pLock->runtime_pm_context);
#endif
#endif
    /*
     * Dont allow autosuspend for these reasons, these reasons doesn't prevent
     * the autosuspend so no need to call allow.
     */
    switch(reason) {
    case WIFI_POWER_EVENT_WAKELOCK_DRIVER_INIT:
    case WIFI_POWER_EVENT_WAKELOCK_DRIVER_REINIT:
        break;
    default:
        vos_runtime_pm_allow_suspend(pLock->runtime_pm_context);
        break;
    }

    return VOS_STATUS_SUCCESS;
}

/*--------------------------------------------------------------------------

  \brief vos_wake_lock_destroy() - destroys a wake lock

  \param pLock - the wake lock to destroy

  \return VOS_STATUS_SUCCESS - the lock was successfully destroyed

  ------------------------------------------------------------------------*/
VOS_STATUS vos_wake_lock_destroy(vos_wake_lock_t *pLock)
{
#if defined CONFIG_CNSS
    cnss_pm_wake_lock_destroy(&pLock->lock);
#elif defined(WLAN_OPEN_SOURCE) && defined(CONFIG_HAS_WAKELOCK)
    wake_lock_destroy(&pLock->lock);
#endif
    vos_runtime_pm_prevent_suspend_deinit(pLock->runtime_pm_context);
    pLock->runtime_pm_context = NULL;
    return VOS_STATUS_SUCCESS;
}

VOS_STATUS vos_runtime_pm_prevent_suspend(runtime_pm_context_t runtime_pm_ctx)
{
	void *ol_sc;
	int ret = 0;

	ol_sc = vos_get_context(VOS_MODULE_ID_HIF,
			vos_get_global_context(VOS_MODULE_ID_SYS, NULL));

	if (ol_sc == NULL) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
				"%s: HIF context is null!", __func__);
		return VOS_STATUS_E_INVAL;
	}

	ret = hif_pm_runtime_prevent_suspend(ol_sc, runtime_pm_ctx);

	if (ret)
		return VOS_STATUS_E_FAILURE;

	return VOS_STATUS_SUCCESS;
}

VOS_STATUS vos_runtime_pm_allow_suspend(runtime_pm_context_t runtime_pm_ctx)
{
	void *ol_sc;
	int ret = 0;

	ol_sc = vos_get_context(VOS_MODULE_ID_HIF,
			vos_get_global_context(VOS_MODULE_ID_SYS, NULL));

	if (ol_sc == NULL) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
				"%s: HIF context is null!", __func__);
		return VOS_STATUS_E_INVAL;
	}

	ret = hif_pm_runtime_allow_suspend(ol_sc, runtime_pm_ctx);

	if (ret)
		return VOS_STATUS_E_FAILURE;

	return VOS_STATUS_SUCCESS;
}

/**
 * vos_runtime_pm_prevent_suspend_timeout() - Prevent runtime suspend timeout
 * msec:	Timeout in milliseconds
 *
 * Prevent runtime suspend with a timeout after which runtime suspend would be
 * allowed. This API uses a single timer to allow the suspend and timer is
 * modified if the timeout is changed before timer fires.
 * If the timeout is less than autosuspend_delay then use mark_last_busy instead
 * of starting the timer.
 *
 * It is wise to try not to use this API and correct the design if possible.
 *
 * Return: VOS_STATUS
 */
VOS_STATUS vos_runtime_pm_prevent_suspend_timeout(runtime_pm_context_t context,
						unsigned int msec)
{
	void *ol_sc;
	int ret = 0;

	ol_sc = vos_get_context(VOS_MODULE_ID_HIF,
			vos_get_global_context(VOS_MODULE_ID_SYS, NULL));

	if (ol_sc == NULL) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
				"%s: HIF context is null!", __func__);
		return VOS_STATUS_E_INVAL;
	}

        ret = hif_pm_runtime_prevent_suspend_timeout(ol_sc, context, msec);
	if (ret)
		return VOS_STATUS_E_FAILURE;

	return VOS_STATUS_SUCCESS;
}

/**
 * vos_runtime_pm_prevent_suspend_init() - Runtime PM Prevent Suspend Ctx init
 * @name: name of the context
 *
 * Through out driver this API should be called to initialize the runtime pm
 * instance.
 *
 * Return: void*
 */
void *vos_runtime_pm_prevent_suspend_init(const char *name)
{
	return hif_runtime_pm_prevent_suspend_init(name);
}

/**
 * vos_runtime_pm_prevent_suspend_deinit() - Runtime PM Prevent context deinit
 * @data: Runtime PM context pointer
 *
 * This API should be called to release the Runtime PM context.
 *
 * Return: void
 */
void vos_runtime_pm_prevent_suspend_deinit(runtime_pm_context_t data)
{
	hif_runtime_pm_prevent_suspend_deinit(data);
}
