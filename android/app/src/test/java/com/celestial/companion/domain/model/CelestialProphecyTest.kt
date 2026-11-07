package com.celestial.companion.domain.model

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class CelestialProphecyTest {

    @Test
    fun `parses full SPELL tag with animation, sound sequence, and clean incantation`() {
        val raw = "[SPELL: Chain Lightning | ANIM: RADIAL_WAVE | SOUNDS: WARP, LASER, PULSAR] Fulgur aetheris catena!"
        val prophecy = CelestialProphecyParser.parse(raw)

        assertTrue(prophecy.isSpell)
        assertEquals("Chain Lightning", prophecy.spellName)
        assertEquals(LedAnimation.RADIAL_WAVE, prophecy.animation)
        assertEquals(listOf(0, 3, 1), prophecy.soundEffects)
        assertEquals("[SPELL] Chain Lightning", prophecy.displayText)
        assertEquals("Fulgur aetheris catena!", prophecy.speechText)
    }

    @Test
    fun `strips think and thought reasoning blocks from spoken TTS output`() {
        val raw = """
            <think>The user wants a protective barrier spell. Let me choose Aegis Shield.</think>
            <thought>Selecting COLLIDING_WAVES and portal sounds.</thought>
            [SPELL: Aegis Shield | ANIM: COLLIDING_WAVES | SOUNDS: PORTAL, TRACTOR, PULSAR]
            Aegis sanctum praesidium, nullus transitus!
        """.trimIndent()

        val prophecy = CelestialProphecyParser.parse(raw)

        assertTrue(prophecy.isSpell)
        assertEquals("Aegis Shield", prophecy.spellName)
        assertEquals(LedAnimation.COLLIDING_WAVES, prophecy.animation)
        assertEquals(listOf(5, 4, 1), prophecy.soundEffects)
        assertTrue(prophecy.thought.contains("protective barrier spell"))
        assertTrue(prophecy.thought.contains("Selecting COLLIDING_WAVES"))
        assertFalse(prophecy.speechText.contains("<think>"))
        assertFalse(prophecy.speechText.contains("protective barrier"))
        assertEquals("Aegis sanctum praesidium, nullus transitus!", prophecy.speechText)
    }

    @Test
    fun `strips unclosed streaming think tag and markdown formatting from oral response`() {
        val rawWithMarkdown = "<think>Analyzing star chart...</think> **Behold** the *ancient* `Celestial` #Engine > of the cosmos~"
        val prophecy = CelestialProphecyParser.parse(rawWithMarkdown)

        assertFalse(prophecy.isSpell)
        assertEquals("Analyzing star chart...", prophecy.thought)
        assertEquals("Behold the ancient Celestial Engine of the cosmos", prophecy.speechText)

        val unclosedThink = "The stars align tonight. <think>Partial reasoning stream..."
        val partialProphecy = CelestialProphecyParser.parse(unclosedThink)
        assertEquals("The stars align tonight.", partialProphecy.speechText)
    }

    @Test
    fun `synthesizes default incantation when SPELL tag has no trailing text`() {
        val raw = "[SPELL: Ouroboros | ANIM: SPIRAL_SNAKE | SOUNDS: ALIEN, PULSAR, WARP]"
        val prophecy = CelestialProphecyParser.parse(raw)

        assertTrue(prophecy.isSpell)
        assertEquals("Ouroboros", prophecy.spellName)
        assertEquals(LedAnimation.SPIRAL_SNAKE, prophecy.animation)
        assertEquals(listOf(2, 1, 0), prophecy.soundEffects)
        assertEquals("Abracadabra Ouroboros invocatus!", prophecy.speechText)
    }
}
