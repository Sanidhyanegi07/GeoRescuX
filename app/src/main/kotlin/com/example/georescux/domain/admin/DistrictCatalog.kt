package com.example.georescux.domain.admin

/**
 * A district inside a state: the finest geographic unit the admin portal
 * aggregates emergencies over (spec §11.2: India → State → District).
 *
 * [bounds] is a coarse lat/lng box used to classify an SOS position into a
 * district. District shapes are irregular, so a point near a border can
 * land in either neighbour; the box is a triage aid, not a cadastral
 * boundary. The raw SOS count remains the ground truth (spec §11.3).
 */
data class District(
    val id: String,
    val displayName: String,
    val stateId: String,
    val minLatitude: Double,
    val maxLatitude: Double,
    val minLongitude: Double,
    val maxLongitude: Double,
) {
    fun contains(latitude: Double, longitude: Double): Boolean =
        latitude in minLatitude..maxLatitude && longitude in minLongitude..maxLongitude
}

/**
 * District reference data for the states GeoRescuX ships. Coordinates are
 * coarse bounding boxes derived from public state/district geography; they
 * are accurate enough for aggregation, not for navigation.
 */
object DistrictCatalog {

    val uttarakhandDistricts = listOf(
        District("dehradun", "Dehradun", "uttarakhand", 30.20, 31.05, 77.45, 78.45),
        District("haridwar", "Haridwar", "uttarakhand", 29.55, 30.45, 77.45, 78.45),
        District("nainital", "Nainital", "uttarakhand", 29.00, 29.75, 78.75, 80.10),
        District("almora", "Almora", "uttarakhand", 29.25, 30.15, 79.25, 80.15),
        District("pithoragarh", "Pithoragarh", "uttarakhand", 29.25, 30.75, 79.55, 81.15),
        District("chamoli", "Chamoli", "uttarakhand", 30.15, 31.15, 78.95, 80.25),
        District("rudraprayag", "Rudraprayag", "uttarakhand", 29.95, 30.75, 78.75, 79.95),
        District("tehri_garhwal", "Tehri Garhwal", "uttarakhand", 29.95, 30.85, 77.95, 79.05),
        District("pauri_garhwal", "Pauri Garhwal", "uttarakhand", 29.65, 30.65, 78.15, 79.45),
        District("bageshwar", "Bageshwar", "uttarakhand", 29.55, 30.45, 79.45, 80.45),
        District("champawat", "Champawat", "uttarakhand", 28.85, 29.65, 79.45, 80.55),
        District("udham_singh_nagar", "Udham Singh Nagar", "uttarakhand", 28.75, 29.45, 78.85, 80.25),
        District("uttarkashi", "Uttarkashi", "uttarakhand", 30.45, 31.25, 77.85, 79.35),
    )

    val himachalPradeshDistricts = listOf(
        District("shimla", "Shimla", "himachal_pradesh", 31.00, 31.35, 77.00, 77.40),
        District("kangra", "Kangra", "himachal_pradesh", 31.80, 32.40, 76.05, 76.85),
        District("kullu", "Kullu", "himachal_pradesh", 31.75, 32.30, 76.95, 77.45),
        District("mandi", "Mandi", "himachal_pradesh", 31.45, 31.90, 76.75, 77.15),
        District("solan", "Solan", "himachal_pradesh", 30.80, 31.20, 76.85, 77.25),
        District("chamba", "Chamba", "himachal_pradesh", 32.25, 33.20, 75.75, 76.75),
        District("sirmaur", "Sirmaur", "himachal_pradesh", 30.40, 31.00, 77.05, 77.85),
        District("hamirpur_hp", "Hamirpur", "himachal_pradesh", 31.55, 31.95, 76.30, 76.70),
        District("una", "Una", "himachal_pradesh", 31.25, 31.75, 76.10, 76.50),
        District("bilaspur_hp", "Bilaspur", "himachal_pradesh", 31.15, 31.60, 76.55, 76.95),
        District("kinnaur", "Kinnaur", "himachal_pradesh", 31.25, 32.15, 77.75, 78.85),
        District("lahaul_spiti", "Lahaul and Spiti", "himachal_pradesh", 31.95, 33.30, 76.55, 78.55),
    )

    val haryanaDistricts = listOf(
        District("gurugram", "Gurugram", "haryana", 28.25, 28.60, 76.75, 77.20),
        District("faridabad", "Faridabad", "haryana", 28.20, 28.55, 77.10, 77.50),
        District("ambala", "Ambala", "haryana", 30.15, 30.65, 76.60, 77.20),
        District("hisar", "Hisar", "haryana", 28.95, 29.45, 75.45, 76.05),
        District("karnal", "Karnal", "haryana", 29.45, 29.95, 76.75, 77.25),
        District("panipat", "Panipat", "haryana", 29.20, 29.60, 76.75, 77.25),
        District("sonipat", "Sonipat", "haryana", 28.80, 29.30, 76.80, 77.25),
        District("rohtak", "Rohtak", "haryana", 28.70, 29.15, 76.35, 76.90),
        District("panchkula", "Panchkula", "haryana", 30.55, 30.95, 76.75, 77.20),
        District("kurukshetra", "Kurukshetra", "haryana", 29.80, 30.25, 76.65, 77.15),
        District("yamunanagar", "Yamunanagar", "haryana", 30.00, 30.45, 77.15, 77.60),
        District("rewari", "Rewari", "haryana", 28.10, 28.40, 76.50, 76.85),
        District("bhiwani", "Bhiwani", "haryana", 28.60, 29.05, 75.95, 76.45),
        District("sirsa", "Sirsa", "haryana", 29.30, 29.90, 74.75, 75.35),
        District("jind", "Jind", "haryana", 29.15, 29.60, 76.10, 76.65),
    )

    val uttarPradeshDistricts = listOf(
        District("lucknow", "Lucknow", "uttar_pradesh", 26.65, 27.15, 80.75, 81.20),
        District("varanasi", "Varanasi", "uttar_pradesh", 25.15, 25.55, 82.80, 83.20),
        District("agra", "Agra", "uttar_pradesh", 27.00, 27.40, 77.80, 78.25),
        District("prayagraj", "Prayagraj", "uttar_pradesh", 25.25, 25.65, 81.65, 82.10),
        District("meerut", "Meerut", "uttar_pradesh", 28.80, 29.20, 77.55, 77.90),
        District("ghaziabad", "Ghaziabad", "uttar_pradesh", 28.55, 28.85, 77.30, 77.60),
        District("noida", "Gautam Buddha Nagar", "uttar_pradesh", 28.30, 28.70, 77.25, 77.65),
        District("kanpur_nagar", "Kanpur Nagar", "uttar_pradesh", 26.30, 26.65, 80.15, 80.50),
        District("bareilly", "Bareilly", "uttar_pradesh", 28.20, 28.60, 79.25, 79.65),
        District("gorakhpur", "Gorakhpur", "uttar_pradesh", 26.60, 26.95, 83.20, 83.60),
        District("ayodhya", "Ayodhya", "uttar_pradesh", 26.65, 26.95, 82.00, 82.35),
        District("mathura", "Mathura", "uttar_pradesh", 27.35, 27.70, 77.50, 77.85),
        District("jhansi", "Jhansi", "uttar_pradesh", 25.30, 25.65, 78.40, 78.75),
        District("aligarh", "Aligarh", "uttar_pradesh", 27.75, 28.10, 77.90, 78.25),
        District("moradabad", "Moradabad", "uttar_pradesh", 28.70, 29.05, 78.60, 78.95),
    )

    /** Every district of the four official states. */
    val allDistricts: List<District> =
        uttarakhandDistricts + himachalPradeshDistricts + haryanaDistricts + uttarPradeshDistricts

    fun districtsForState(stateId: String): List<District> =
        allDistricts.filter { it.stateId == stateId }

    /** The district containing the position, or null when it lies outside the shipped districts. */
    fun districtForLocation(latitude: Double, longitude: Double): District? =
        allDistricts.firstOrNull { it.contains(latitude, longitude) }
}
