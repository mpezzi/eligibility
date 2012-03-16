<?php

/**
 * @file
 * Provides eligibility resource for services module.
 */


/**
 * Return an eligibility object for the current user.
 */
function _eligibility_resource_get($tid = 0) {
  return eligibility_get(array('uid' => $GLOBALS['user']->uid, 'tid' => $tid));
}

/**
 * Save an eligibility event.
 */
function _eligibility_resource_event($tid = 0) {
  eligibility_event_save($GLOBALS['user']->uid, $tid);
}

/**
 * Determine whether the current user can access an eligibility resource.
 *
 * @param $op
 *   An operation type.
 * @param $args
 *   Resource arguments passed through from the original request.
 * @return
 *   Boolean TRUE if the current user has the requested permissions.
 */
function _eligibility_resource_access($op = 'get', $args = array()) {
  if ( user_is_logged_in() ) {
    if ( $op == 'get' || $op == 'event' ) {
      return TRUE;
    }
  }

  return FALSE;
}
