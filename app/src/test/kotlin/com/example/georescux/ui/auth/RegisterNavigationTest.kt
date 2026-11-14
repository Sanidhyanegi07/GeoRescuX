package com.example.georescux.ui.auth

import com.example.georescux.ui.home.HomeActivity
import com.example.georescux.ui.region.RegionSelectionActivity
import org.junit.Assert.assertEquals
import org.junit.Test

class RegisterNavigationTest {

    @Test
    fun `registration routes to RegionSelectionActivity when region is not selected`() {
        val destination = RegisterActivity.resolvePostRegistrationDestination(hasSelectedRegion = false)
        assertEquals(RegionSelectionActivity::class.java, destination)
    }

    @Test
    fun `registration routes to HomeActivity when region is already selected`() {
        val destination = RegisterActivity.resolvePostRegistrationDestination(hasSelectedRegion = true)
        assertEquals(HomeActivity::class.java, destination)
    }
}

