package com.celestial.companion.ai

import com.celestial.companion.domain.model.LedAnimation
import com.celestial.companion.domain.model.SpellDefinition
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class FastSpellMatcherTest {

    @Test
    fun `matches English spell keywords accurately`() {
        assertEquals("Fireball", FastSpellMatcher.match("Cast a giant fireball now!")?.name)
        assertEquals("Chain Lightning", FastSpellMatcher.match("Strike with chain lightning thunderbolt")?.name)
        assertEquals("Aegis Shield", FastSpellMatcher.match("Activate Aegis shield defense")?.name)
        assertEquals("Ouroboros Serpent", FastSpellMatcher.match("Summon the Ouroboros serpent")?.name)
        assertEquals("Quantum Quack", FastSpellMatcher.match("Release the rainbow quantum quack duck")?.name)
        assertEquals("Lumos Maxima", FastSpellMatcher.match("Lumos Maxima!")?.name)
        assertEquals("Expecto Patronum", FastSpellMatcher.match("Expecto Patronum against the dementor")?.name)
        assertEquals("eBay Chaos Bid", FastSpellMatcher.match("Start the eBay auction cha-ching")?.name)
        assertEquals("Moon Prism Power", FastSpellMatcher.match("Sailor Moon prism power make up")?.name)
    }

    @Test
    fun `matches Russian spell keywords accurately`() {
        assertEquals("Fireball", FastSpellMatcher.match("Запусти огненный шар и пламя")?.name)
        assertEquals("Chain Lightning", FastSpellMatcher.match("Шарахни цепная молния и гром")?.name)
        assertEquals("Aegis Shield", FastSpellMatcher.match("Поставь щит эгиды и барьер")?.name)
        assertEquals("Ouroboros Serpent", FastSpellMatcher.match("Призови уроборос змейку")?.name)
        assertEquals("Quantum Quack", FastSpellMatcher.match("Квантовая утка крякни радуга")?.name)
        assertEquals("Lumos Maxima", FastSpellMatcher.match("Люмос максима!")?.name)
        assertEquals("Expecto Patronum", FastSpellMatcher.match("Экспекто патронум!")?.name)
        assertEquals("eBay Chaos Bid", FastSpellMatcher.match("Открой ибей аукцион")?.name)
        assertEquals("Moon Prism Power", FastSpellMatcher.match("Сейлор Мун лунная призма дай мне силу")?.name)
    }

    @Test
    fun `matches German spell keywords accurately`() {
        assertEquals("Fireball", FastSpellMatcher.match("Wirf einen Feuerball mit Flamme")?.name)
        assertEquals("Chain Lightning", FastSpellMatcher.match("Kettenblitz und Donner!")?.name)
        assertEquals("Aegis Shield", FastSpellMatcher.match("Aktiviere Schild und Barriere")?.name)
        assertEquals("Ouroboros Serpent", FastSpellMatcher.match("Beschwöre die Ouroboros Schlange")?.name)
        assertEquals("Quantum Quack", FastSpellMatcher.match("Regenbogen Quantenente quak")?.name)
        assertEquals("Lumos Maxima", FastSpellMatcher.match("Lumos Zauberstab Licht")?.name)
        assertEquals("Expecto Patronum", FastSpellMatcher.match("Expecto Patronum Schutzpatron")?.name)
        assertEquals("eBay Chaos Bid", FastSpellMatcher.match("Starte die eBay Auktion Gebot")?.name)
        assertEquals("Moon Prism Power", FastSpellMatcher.match("Sailor Moon Mondprisma Macht")?.name)
    }

    @Test
    fun `returns null for conversational non-spell queries and avoids partial-word false positives`() {
        assertNull(FastSpellMatcher.match(""))
        assertNull(FastSpellMatcher.match("   "))
        assertNull(FastSpellMatcher.match("What is the weather in Berlin today?"))
        assertNull(FastSpellMatcher.match("Your voice sounds very clear on the microphone")) // "voice" must not match "ice"
        assertNull(FastSpellMatcher.match("Я вижу странный след на песке и огромный камень")) // "след"/"огромный" must not match "лед"/"гром"
        assertNull(FastSpellMatcher.match("Wie spät ist es auf der Uhr?"))
    }

    @Test
    fun `prioritizes custom user spells before built-in fast spells`() {
        val customSpell = SpellDefinition(
            name = "Supernova Overdrive",
            animation = LedAnimation.PULSAR,
            sounds = listOf(6, 1),
            prompt = "Custom supernova!"
        )
        val matched = FastSpellMatcher.match("Please trigger Supernova Overdrive", listOf(customSpell))
        assertNotNull(matched)
        assertEquals("Supernova Overdrive", matched?.name)
    }

    @Test
    fun `executes spell matching well under 20ms latency budget`() {
        val queries = listOf(
            "Cast Fireball!",
            "Цепная молния!",
            "Aktiviere Schild!",
            "Ouroboros serpent",
            "Quantum quack duck",
            "Lumos Maxima",
            "Expecto Patronum",
            "eBay auction",
            "Sailor Moon prism",
            "How many LEDs does the staff headpiece have?"
        )
        val startNs = System.nanoTime()
        for (q in queries) {
            FastSpellMatcher.match(q)
        }
        val elapsedMs = (System.nanoTime() - startNs) / 1_000_000.0
        assertTrue("Expected < 20ms for batch spell matching, took ${elapsedMs}ms", elapsedMs < 20.0)
    }
}
