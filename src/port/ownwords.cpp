#include "port/ownwords.h"

#include <iterator>

#include "romdata.h"

namespace sims2wide
{
namespace
{
// English, French, German, Italian, Spanish: the port's own words (not the game's), kept short:
// most sit beside a button's chip.
constexpr const char* kWords[][5] = {
    // Point, Slow
    {"Point", "Viser", "Zeigen", "Punta", "Apuntar"},
    {"Slow", "Lent", "Langsam", "Lento", "Lento"},
    // Autopsy: the tools
    {"Laser", "Laser", "Laser", "Laser", "Láser"},
    {"Hand", "Main", "Hand", "Mano", "Mano"},
    {"Tweezers", "Pince", "Pinzette", "Pinzette", "Pinzas"},
    // What A does with each
    {"Hold: cut", "Maintenir : couper", "Halten: schneiden", "Tieni: taglia", "Mantén: cortar"},
    {"Hold: lift an organ", "Maintenir : prendre un organe", "Halten: Organ heben", "Tieni: prendi un organo", "Mantén: coger un órgano"},
    {"Hold: pick a parasite", "Maintenir : prendre un parasite", "Halten: Parasit greifen", "Tieni: prendi un parassita", "Mantén: coger un parásito"},
    // The carry, the tear
    {"To the tray", "Vers le plateau", "Zur Schale", "Al vassoio", "A la bandeja"},
    {"To the flask", "Vers le bocal", "Zum Glas", "Al barattolo", "Al frasco"},
    {"Hold and pull: tear it open", "Maintenir et tirer : déchirer", "Halten und ziehen: aufreißen", "Tieni e tira: strappa", "Mantén y tira: rasgar"},
    // Quit, asked once
    {"Quit", "Quitter", "Beenden", "Esci", "Salir"},
    {"Again: quit, the earnings lost", "Encore : quitter, gains perdus", "Nochmal: beenden, Geld verloren", "Ancora: esci, guadagni persi", "Otra vez: salir sin ganancias"},
    // The autopsy's values
    {"Specimen @1 of 2", "Spécimen @1 sur 2", "Exemplar @1 von 2", "Esemplare @1 di 2", "Espécimen @1 de 2"},
    {"Organ @1", "Organe @1", "Organ @1", "Organo @1", "Órgano @1"},
    {"Freed", "Libéré", "Gelöst", "Libero", "Suelto"},
    {"Removed", "Retiré", "Entfernt", "Rimosso", "Extraído"},
    {"Destroyed", "Détruit", "Zerstört", "Distrutto", "Destruido"},
    {"Parasites", "Parasites", "Parasiten", "Parassiti", "Parásitos"},
    {"@1 eating", "@1 mangent", "@1 fressen", "@1 mangiano", "@1 comiendo"},
    // The detector
    {"Walk", "Marcher", "Gehen", "Cammina", "Andar"},
    {"Dig", "Creuser", "Graben", "Scava", "Cavar"},
    {"Depth", "Profondeur", "Tiefe", "Profondità", "Profundidad"},
    {"Power off", "Éteindre", "Ausschalten", "Spegni", "Apagar"},
    {"Charge", "Charge", "Ladung", "Carica", "Carga"},
    {"Signal", "Signal", "Signal", "Segnale", "Señal"},
    {"None", "Aucun", "Keins", "Nessuno", "Ninguna"},
    // The painting: the hints
    {"Hold: paint", "Maintenir : peindre", "Halten: malen", "Tieni: dipingi", "Mantén: pintar"},
    {"Tool", "Outil", "Werkzeug", "Strumento", "Herramienta"},
    {"Next color", "Couleur suivante", "Nächste Farbe", "Colore seguente", "Color siguiente"},
    {"Undo", "Annuler", "Rückgängig", "Annulla", "Deshacer"},
    {"Done", "Terminé", "Fertig", "Fatto", "Hecho"},
    // The tools 0..5
    {"Pencil", "Crayon", "Stift", "Matita", "Lápiz"},
    {"Eraser", "Gomme", "Radierer", "Gomma", "Goma"},
    {"Line", "Ligne", "Linie", "Linea", "Línea"},
    {"Bucket", "Pot de peinture", "Farbeimer", "Secchiello", "Bote"},
    {"Rectangle", "Rectangle", "Rechteck", "Rettangolo", "Rectángulo"},
    {"Filled rectangle", "Rectangle plein", "Volles Rechteck", "Rettangolo pieno", "Rectángulo relleno"},
    // The brush sizes
    {"small", "petit", "klein", "piccolo", "pequeño"},
    {"medium", "moyen", "mittel", "medio", "mediano"},
    {"large", "grand", "groß", "grande", "grande"},
    // The colour and page
    {"Color @1 of 15", "Couleur @1 sur 15", "Farbe @1 von 15", "Colore @1 di 15", "Color @1 de 15"},
    {"Palette @1 of 16", "Palette @1 sur 16", "Palette @1 von 16", "Tavolozza @1 di 16", "Paleta @1 de 16"},
    // The cards
    {"Continue", "Continuer", "Weiter", "Continua", "Continuar"},
    {"Choose a card", "Choisir une carte", "Karte wählen", "Scegli una carta", "Elegir carta"},
    {"Pick up, again to play", "Prendre, encore : jouer", "Nehmen, nochmal: spielen", "Prendi, ancora: gioca", "Coger, otra vez: jugar"},
    {"Draw a card", "Piocher", "Karte ziehen", "Pesca", "Robar carta"},
    {"Discard", "Défausser", "Abwerfen", "Scarta", "Descartar"},
    // The vacuum
    {"Drive", "Conduire", "Fahren", "Guida", "Conducir"},
    {"Move the hand", "Bouger la main", "Hand bewegen", "Muovi la mano", "Mover la mano"},
    {"Grab, drag to the box", "Saisir, glisser dans la boîte", "Greifen, in die Box ziehen", "Prendi, trascina nella scatola", "Coger, llevar a la caja"},
    {"Blow: unclog", "Souffler : déboucher", "Pusten: frei machen", "Soffia: sblocca", "Soplar: desatascar"},
    {"Blow: unclog!", "Souffler : déboucher !", "Pusten: frei machen!", "Soffia: sblocca!", "Soplar: ¡desatascar!"},
    // The social games
    {"Answer", "Répondre", "Antworten", "Rispondi", "Responder"},
    {"The chosen answer again", "La même réponse", "Dieselbe Antwort", "La stessa risposta", "La misma respuesta"},
    {"Match the gesture", "Imite le geste", "Geste nachmachen", "Imita il gesto", "Imita el gesto"},
    // MooGoo Monkey
    {"Play", "Jouer", "Spielen", "Gioca", "Jugar"},
    {"Choose a monkey", "Choisir un singe", "Affen wählen", "Scegli una scimmia", "Elegir mono"},
    {"Bet", "Parier", "Wetten", "Punta", "Apostar"},
    {"Play the card", "Jouer la carte", "Karte spielen", "Gioca la carta", "Jugar la carta"},
    {"Undo the bet", "Annuler le pari", "Wette zurück", "Annulla la puntata", "Deshacer apuesta"},
    {"Play on it", "Jouer dessus", "Darauf spielen", "Gioca qui", "Jugar aquí"},
    {"Take it back", "Reprendre", "Zurücknehmen", "Riprendi", "Recuperar"},
    {"Players", "Joueurs", "Spieler", "Giocatori", "Jugadores"},
    {"OK", "OK", "OK", "OK", "Aceptar"},
    {"Round @1 of 3", "Manche @1 sur 3", "Runde @1 von 3", "Round @1 di 3", "Ronda @1 de 3"},
    {"Cards left", "Cartes restantes", "Karten übrig", "Carte rimaste", "Cartas restantes"},
    // The Super Drencher
    {"Hold: fire", "Maintenir : tirer", "Halten: spritzen", "Tieni: spara", "Mantén: disparar"},
    {"Hold: pump", "Maintenir : pomper", "Halten: pumpen", "Tieni: pompa", "Mantén: bombear"},
    {"Target", "Cible", "Ziel", "Bersaglio", "Objetivo"},
    {"Refill", "Remplir", "Auffüllen", "Ricarica", "Rellenar"},
    {"Run", "Courir", "Rennen", "Corri", "Correr"},
    {"Put away", "Ranger", "Wegstecken", "Metti via", "Guardar"},
    {"Water", "Eau", "Wasser", "Acqua", "Agua"},
    {"Pressure", "Pression", "Druck", "Pressione", "Presión"},
    {"None", "Aucune", "Keins", "Nessuno", "Ninguno"},
    // The crime game
    {"Scare", "Effrayer", "Erschrecken", "Spaventa", "Asustar"},
    {"Hold: aim", "Maintenir : viser", "Halten: zielen", "Tieni: mira", "Mantén: apuntar"},
    {"Let go: throw", "Relâcher : lancer", "Loslassen: werfen", "Rilascia: lancia", "Suelta: lanzar"},
    {"Subdue", "Maîtriser", "Überwältigen", "Immobilizza", "Reducir"},
    {"Ready", "Prêt", "Bereit", "Pronto", "Listo"},
    {"Aim", "Visée", "Ziel", "Mira", "Puntería"},
    {"Hit!", "Touché !", "Treffer!", "Colpito!", "¡Tocado!"},
    // The Scratch lottery: A, X, B asked once, the prize, the boxes
    {"Hold: scratch", "Maintenir : gratter", "Halten: rubbeln", "Tieni: gratta", "Mantén: rascar"},
    {"Scratch this box", "Gratter la case", "Feld freirubbeln", "Gratta la casella", "Rascar la casilla"},
    {"Again: quit, the ticket lost", "Encore : quitter, ticket perdu", "Nochmal: beenden, Los verloren", "Ancora: esci, biglietto perso", "Otra vez: salir, boleto perdido"},
    {"Top prize", "Gros lot", "Hauptgewinn", "Primo premio", "Premio mayor"},
    {"@1 of 5 scratched", "@1 sur 5 grattées", "@1 von 5 gerubbelt", "@1 di 5 grattate", "@1 de 5 rascadas"},
    // The music mixer: the hints
    {"Hold: touch", "Maintenir : toucher", "Halten: tippen", "Tieni: tocca", "Mantén: tocar"},
    {"Touch", "Toucher", "Tippen", "Tocca", "Tocar"},
    {"Octave", "Octave", "Oktave", "Ottava", "Octava"},
    {"Sustain", "Tenue", "Halten", "Sostegno", "Sostener"},
    {"Stop", "Arrêter", "Stopp", "Ferma", "Parar"},
    {"Hold: VOX (mic)", "Maintenir : VOX (micro)", "Halten: VOX (Mikro)", "Tieni: VOX (micro)", "Mantén: VOX (micro)"},
    {"Let go: stop", "Relâcher : arrêter", "Loslassen: stopp", "Rilascia: ferma", "Suelta: parar"},
    {"Choose", "Choisir", "Wählen", "Scegli", "Elegir"},
    {"Back", "Retour", "Zurück", "Indietro", "Atrás"},
    {"Octave, sustain, @1", "Octave, tenue, @1", "Oktave, halten, @1", "Ottava, sostegno, @1", "Octava, sostener, @1"},
    // The mixer's values
    {"Song", "Morceau", "Song", "Brano", "Canción"},
    {"Instrument", "Instrument", "Instrument", "Strumento", "Instrumento"},
    {"Ready", "Prêt", "Bereit", "Pronto", "Listo"},
    {"Playing", "Lecture", "Spielt", "In riproduzione", "Sonando"},
    {"Recording", "Enregistrement", "Aufnahme", "Registrazione", "Grabando"},
    {"Help", "Aide", "Hilfe", "Aiuto", "Ayuda"},
    // The save page
    {"Type", "Écrire", "Tippen", "Scrivi", "Escribir"},
    {"Save here", "Enregistrer ici", "Hier speichern", "Salva qui", "Guardar aquí"},
    {"Again: save over it", "Encore : écraser", "Nochmal: überschreiben", "Ancora: sovrascrivi", "Otra vez: sobrescribir"},
    {"Erase", "Effacer", "Löschen", "Cancella", "Borrar"},
    {"Slot @1", "Emplacement @1", "Platz @1", "Spazio @1", "Espacio @1"},
    // The wireless lobby
    {"Wireless play needs a second DS: not in this port.", "Le jeu sans fil demande une deuxième DS : absent de ce portage.",
     "Drahtlos-Spiel braucht eine zweite DS: nicht in dieser Portierung.", "Il gioco wireless richiede un secondo DS: non in questo port.",
     "El juego inalámbrico necesita otra DS: no está en esta versión."},
    // A help text
    {"Close", "Fermer", "Schließen", "Chiudi", "Cerrar"},
    // The town's menu hint
    {"Menu", "Menu", "Menü", "Menu", "Menú"},
    // Controllers
    {"@1: @2 opens the pause menu", "@1 : @2 ouvre le menu pause", "@1: @2 öffnet das Pausenmenü", "@1: @2 apre il menu di pausa",
     "@1: @2 abre el menú de pausa"},
    {"Pause", "Pause", "Pause", "Pausa", "Pausa"},
    // The painting's palette page, a minigame's help, the stick cursor
    {"Next palette", "Palette suivante", "Nächste Palette", "Tavolozza seguente", "Paleta siguiente"},
    {"What it does", "À quoi ça sert", "Was es tut", "A cosa serve", "Para qué sirve"},
    {"Leave help", "Quitter l'aide", "Hilfe beenden", "Esci dall'aiuto", "Salir de la ayuda"},
    {"Cursor", "Curseur", "Cursor", "Cursore", "Cursor"},
    // The menus
    {"Select", "Choisir", "Wählen", "Scegli", "Elegir"},
    {"Again: throw it away", "Encore : le jeter", "Nochmal: wegwerfen", "Ancora: buttalo", "Otra vez: tirarlo"},
    {"No goals for now.", "Aucun objectif pour l'instant.", "Zurzeit keine Ziele.", "Nessun obiettivo per ora.", "Ningún objetivo por ahora."},
    {"Nothing to do here.", "Rien à faire ici.", "Hier gibt es nichts zu tun.", "Niente da fare qui.", "Aquí no hay nada que hacer."},
    // MooGoo's bet
    {"Place a bet on a monkey!", "Pariez sur un singe !", "Setz auf einen Affen!", "Scommetti su una scimmia!", "¡Apuesta por un mono!"},
};
static_assert(std::size(kWords) == (size_t)OwnWordId::Count, "one row per OwnWordId");
}

const char* OwnWordIn(int language, OwnWordId id)
{
    const int i = (int)id;
    if (i < 0 || i >= (int)OwnWordId::Count) return "";
    return kWords[i][language >= 1 && language <= 5 ? language - 1 : 0];
}

const char* OwnWord(const RomData* rom, OwnWordId id) { return OwnWordIn(rom ? rom->Language() : 1, id); }
}
