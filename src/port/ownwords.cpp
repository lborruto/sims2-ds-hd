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
    // The HD shell: the pause menu's title and pages, its buttons' words
    {"Paused", "Pause", "Pause", "Pausa", "Pausa"},
    {"Resume", "Reprendre", "Weiter", "Riprendi", "Continuar"},
    {"Settings", "Réglages", "Einstellungen", "Impostazioni", "Ajustes"},
    {"Controls", "Commandes", "Steuerung", "Comandi", "Controles"},
    {"Save states", "Sauvegardes rapides", "Spielstände", "Salvataggi rapidi", "Estados guardados"},
    {"Quit", "Quitter", "Beenden", "Esci", "Salir"},
    {"Back", "Retour", "Zurück", "Indietro", "Volver"},
    {"Change", "Changer", "Ändern", "Cambia", "Cambiar"},
    {"Open", "Ouvrir", "Öffnen", "Apri", "Abrir"},
    {"Yes", "Oui", "Ja", "Sì", "Sí"},
    {"No", "Non", "Nein", "No", "No"},
    {"OK", "OK", "OK", "OK", "Aceptar"},
    {"On", "Oui", "An", "Sì", "Sí"},
    {"Off", "Non", "Aus", "No", "No"},
    {"Pause menu", "Menu pause", "Pausenmenü", "Menu di pausa", "Menú de pausa"},
    // The settings' rows
    {"Layout", "Disposition", "Anordnung", "Disposizione", "Disposición"},
    {"Fullscreen", "Plein écran", "Vollbild", "Schermo intero", "Pantalla completa"},
    {"Picture filter", "Filtre d'image", "Bildfilter", "Filtro immagine", "Filtro de imagen"},
    {"3D resolution", "Résolution 3D", "3D-Auflösung", "Risoluzione 3D", "Resolución 3D"},
    {"Widescreen world", "Monde en 16:9", "Welt in 16:9", "Mondo in 16:9", "Mundo en 16:9"},
    {"60 fps", "60 i/s", "60 fps", "60 fps", "60 fps"},
    {"Volume", "Volume", "Lautstärke", "Volume", "Volumen"},
    {"Sound in fast forward", "Son en avance rapide", "Ton im Schnelllauf", "Audio in avanti veloce", "Sonido en avance rápido"},
    {"Stick cursor", "Curseur au stick", "Stick-Cursor", "Cursore con levetta", "Cursor con stick"},
    {"Stick dead zone", "Zone morte du stick", "Stick-Totzone", "Zona morta levetta", "Zona muerta del stick"},
    {"Trigger press at", "Gâchette pressée à", "Trigger ab", "Grilletto premuto al", "Gatillo pulsado al"},
    {"Rumble", "Vibration", "Vibration", "Vibrazione", "Vibración"},
    {"Microphone", "Micro", "Mikrofon", "Microfono", "Micrófono"},
    {"Language", "Langue", "Sprache", "Lingua", "Idioma"},
    {"Blow key", "Touche souffle", "Pustetaste", "Tasto soffio", "Tecla de soplido"},
    {"Automatic", "Automatique", "Automatisch", "Automatica", "Automático"},
    {"The game's language and the HD screens'. Applies the next time the game starts.",
     "La langue du jeu et des écrans HD. Prise en compte au prochain lancement du jeu.",
     "Sprache des Spiels und der HD-Bildschirme. Gilt ab dem nächsten Spielstart.",
     "La lingua del gioco e delle schermate HD. Vale dal prossimo avvio del gioco.",
     "El idioma del juego y de las pantallas HD. Se aplica al volver a iniciar el juego."},
    // The layouts
    {"Console", "Console", "Konsole", "Console", "Consola"},
    {"Auto", "Auto", "Auto", "Auto", "Auto"},
    {"Focus", "Focus", "Fokus", "Focus", "Foco"},
    {"Side by side", "Côte à côte", "Nebeneinander", "Affiancati", "Lado a lado"},
    {"Stacked", "Empilés", "Übereinander", "Impilati", "Apiladas"},
    // The controls card
    {"Walk, move", "Marcher, déplacer", "Gehen, bewegen", "Cammina, muovi", "Andar, mover"},
    {"Touch, confirm", "Toucher, valider", "Tippen, bestätigen", "Tocca, conferma", "Tocar, aceptar"},
    {"Back, close", "Retour, fermer", "Zurück, schließen", "Indietro, chiudi", "Volver, cerrar"},
    {"Zoom in / out", "Zoom avant / arrière", "Zoom rein / raus", "Zoom avanti / indietro", "Acercar / alejar"},
    {"Turn the camera", "Tourner la caméra", "Kamera drehen", "Ruota la visuale", "Girar la cámara"},
    {"Fast forward", "Avance rapide", "Schnelllauf", "Avanti veloce", "Avance rápido"},
    {"Keys and buttons", "Touches et boutons", "Tasten und Knöpfe", "Tasti e pulsanti", "Teclas y botones"},
    {"Blow (the vacuum)", "Souffler (l'aspirateur)", "Pusten (der Sauger)", "Soffia (l'aspirapolvere)", "Soplar (la aspiradora)"},
    // The bindings' actions (the DS's buttons by their letters)
    {"D-pad right", "Croix : droite", "Steuerkreuz rechts", "Croce: destra", "Cruceta: derecha"},
    {"D-pad left", "Croix : gauche", "Steuerkreuz links", "Croce: sinistra", "Cruceta: izquierda"},
    {"D-pad up", "Croix : haut", "Steuerkreuz hoch", "Croce: su", "Cruceta: arriba"},
    {"D-pad down", "Croix : bas", "Steuerkreuz runter", "Croce: giù", "Cruceta: abajo"},
    {"Swap screens", "Échanger les écrans", "Bildschirme tauschen", "Scambia schermi", "Cambiar pantallas"},
    {"Quick save state", "Sauvegarde rapide", "Schnellspeichern", "Salvataggio rapido", "Guardado rápido"},
    {"Quick load state", "Chargement rapide", "Schnellladen", "Caricamento rapido", "Carga rápida"},
    {"Blow (microphone)", "Souffler (micro)", "Pusten (Mikrofon)", "Soffia (microfono)", "Soplar (micrófono)"},
    {"Touch at the cursor", "Toucher au curseur", "Am Cursor tippen", "Tocca al cursore", "Tocar en el cursor"},
    // Changing a binding
    {"Press a key or a button: @1", "Appuyez sur une touche ou un bouton : @1", "Taste oder Knopf drücken: @1", "Premi un tasto o un pulsante: @1",
     "Pulsa una tecla o un botón: @1"},
    {"Esc or Start + Back: keep it. Backspace: no key.", "Échap ou Start + Back : garder. Retour arrière : aucune touche.",
     "Esc oder Start + Back: behalten. Rücktaste: keine Taste.", "Esc o Start + Back: mantieni. Backspace: nessun tasto.",
     "Esc o Start + Back: mantener. Retroceso: sin tecla."},
    {"Clear", "Effacer", "Löschen", "Cancella", "Borrar"},
    {"All in turn", "Toutes à la suite", "Alle nacheinander", "Tutti in fila", "Todas seguidas"},
    {"Reset to defaults", "Valeurs par défaut", "Standard wiederherstellen", "Ripristina predefiniti", "Restablecer"},
    {"Keyboard", "Clavier", "Tastatur", "Tastiera", "Teclado"},
    {"Controller", "Manette", "Controller", "Controller", "Mando"},
    // The save states
    {"Empty", "Vide", "Leer", "Vuoto", "Vacío"},
    {"Save here", "Sauver ici", "Hier speichern", "Salva qui", "Guardar aquí"},
    {"Load", "Charger", "Laden", "Carica", "Cargar"},
    {"Load slot @1? What you played since then is lost (Undo brings it back).",
     "Charger l'emplacement @1 ? Ce que vous avez joué depuis est perdu (Annuler le rend).",
     "Platz @1 laden? Was du seitdem gespielt hast, geht verloren (Rückgängig holt es zurück).",
     "Caricare lo spazio @1? Ciò che hai giocato da allora va perso (Annulla lo riporta).",
     "¿Cargar el espacio @1? Lo jugado desde entonces se pierde (Deshacer lo recupera)."},
    {"Save over slot @1?", "Écraser l'emplacement @1 ?", "Platz @1 überschreiben?", "Sovrascrivere lo spazio @1?", "¿Sobrescribir el espacio @1?"},
    {"Undo the last load", "Annuler le dernier chargement", "Letztes Laden rückgängig", "Annulla l'ultimo caricamento", "Deshacer la última carga"},
    {"Slot @1 saved", "Emplacement @1 sauvé", "Platz @1 gespeichert", "Spazio @1 salvato", "Espacio @1 guardado"},
    {"Slot @1 loaded", "Emplacement @1 chargé", "Platz @1 geladen", "Spazio @1 caricato", "Espacio @1 cargado"},
    {"Back to before the load", "Retour avant le chargement", "Zurück vor das Laden", "Tornato a prima del caricamento", "De vuelta a antes de la carga"},
    {"That did not work", "Cela n'a pas marché", "Das ging nicht", "Non ha funzionato", "No ha funcionado"},
    {"No folder for save states", "Aucun dossier pour les sauvegardes", "Kein Ordner für Spielstände", "Nessuna cartella per i salvataggi",
     "Sin carpeta para estados"},
    // Quitting
    {"Quit the game?", "Quitter le jeu ?", "Spiel beenden?", "Uscire dal gioco?", "¿Salir del juego?"},
    {"Your place is kept: Continue picks up here next time.", "Votre partie est gardée : Continuer reprend ici la prochaine fois.",
     "Dein Stand bleibt: Weiter macht nächstes Mal hier weiter.", "Il punto è conservato: Continua riparte da qui la prossima volta.",
     "Se guarda tu sitio: Continuar sigue aquí la próxima vez."},
    // The toasts
    {"State saved", "État sauvé", "Stand gespeichert", "Stato salvato", "Estado guardado"},
    {"State loaded", "État chargé", "Stand geladen", "Stato caricato", "Estado cargado"},
    {"No state to load", "Aucun état à charger", "Kein Stand zum Laden", "Nessuno stato da caricare", "Ningún estado que cargar"},
    {"Screenshot saved", "Capture d'écran enregistrée", "Bildschirmfoto gespeichert", "Screenshot salvato", "Captura guardada"},
    {"Save backed up", "Sauvegarde copiée", "Spielstand gesichert", "Salvataggio copiato", "Partida copiada"},
    {"New screen recorded for the port", "Nouvel écran enregistré pour le portage", "Neuer Bildschirm für die Portierung aufgenommen",
     "Nuova schermata registrata per il port", "Nueva pantalla grabada para la versión"},
    {"The game cannot write its save: @1", "Le jeu ne peut pas écrire sa sauvegarde : @1", "Das Spiel kann nicht speichern: @1",
     "Il gioco non riesce a salvare: @1", "El juego no puede guardar: @1"},
    {"Not the European cartridge (@1): the HD screens need it. The game plays with its own screens.",
     "Pas la cartouche européenne (@1) : les écrans HD en ont besoin. Le jeu garde ses propres écrans.",
     "Nicht das europäische Modul (@1): die HD-Bildschirme brauchen es. Das Spiel zeigt seine eigenen.",
     "Non è la cartuccia europea (@1): le schermate HD ne hanno bisogno. Il gioco usa le sue.",
     "No es el cartucho europeo (@1): las pantallas HD lo necesitan. El juego usa las suyas."},
    // The launch panel
    {"Continue", "Continuer", "Weiter", "Continua", "Continuar"},
    {"Where you left off, @1", "Là où vous en étiez, @1", "Wo du aufgehört hast, @1", "Dove eri rimasto, @1", "Donde lo dejaste, @1"},
    {"Choose a file", "Choisir un fichier", "Datei wählen", "Scegli un file", "Elegir un archivo"},
    {"Recent", "Récents", "Zuletzt", "Recenti", "Recientes"},
    {"Or drop a .nds file on this window", "Ou déposez un fichier .nds sur cette fenêtre", "Oder eine .nds-Datei auf dieses Fenster ziehen",
     "Oppure trascina un file .nds su questa finestra", "O suelta un archivo .nds en esta ventana"},
    {"@1 is not The Sims 2 for the DS. Choose your copy of the game.", "@1 n'est pas Les Sims 2 sur DS. Choisissez votre copie du jeu.",
     "@1 ist nicht Die Sims 2 für den DS. Wähle deine Kopie des Spiels.", "@1 non è The Sims 2 per DS. Scegli la tua copia del gioco.",
     "@1 no es Los Sims 2 para DS. Elige tu copia del juego."},
    {"Cannot read @1.", "Impossible de lire @1.", "@1 lässt sich nicht lesen.", "Impossibile leggere @1.", "No se puede leer @1."},
    {"Start", "Démarrer", "Starten", "Avvia", "Empezar"},
    {"The Sims 2 DS, one screen", "Les Sims 2 DS, un seul écran", "Die Sims 2 DS, ein Bildschirm", "The Sims 2 DS, uno schermo", "Los Sims 2 DS, una pantalla"},
    // DS Wireless on the main menu
    {"Needs a second DS: not in this port.", "Demande une deuxième DS : absent de ce portage.", "Braucht eine zweite DS: nicht in dieser Portierung.",
     "Serve un secondo DS: non in questo port.", "Necesita otra DS: no está en esta versión."},
    // Change game, the saves' folder
    {"Change game", "Changer de jeu", "Spiel wechseln", "Cambia gioco", "Cambiar de juego"},
    {"Open the saves folder", "Ouvrir le dossier des sauvegardes", "Speicherordner öffnen", "Apri la cartella dei salvataggi",
     "Abrir la carpeta de partidas"},
    // Continue refused: the game saved after the place was kept
    {"The game was saved after this place was kept: it starts from its own save.",
     "Le jeu a été sauvegardé après ce point : il reprend depuis sa propre sauvegarde.",
     "Das Spiel wurde danach gespeichert: es startet vom eigenen Spielstand.",
     "Il gioco è stato salvato dopo questo punto: riparte dal suo salvataggio.",
     "El juego se guardó después de este punto: empieza desde su propia partida."},
    // A state made with another file
    {"Made with another file (@1): its save would go into this one.", "Fait avec un autre fichier (@1) : sa sauvegarde irait dans celui-ci.",
     "Mit einer anderen Datei gemacht (@1): ihr Spielstand käme in diese.", "Fatto con un altro file (@1): il suo salvataggio finirebbe in questo.",
     "Hecho con otro archivo (@1): su partida iría a este."},
    // Continue failed
    {"Your place could not be loaded: the game starts from its own save.", "Votre partie n'a pas pu être chargée : le jeu reprend depuis sa sauvegarde.",
     "Dein Stand ließ sich nicht laden: das Spiel startet vom eigenen Spielstand.", "Impossibile caricare il punto: il gioco riparte dal suo salvataggio.",
     "No se pudo cargar tu sitio: el juego empieza desde su propia partida."},
    // Platform: the settings' new rows
    {"Battery saver", "Économie de batterie", "Akku sparen", "Risparmio batteria", "Ahorro de batería"},
    {"Mute", "Muet", "Stumm", "Muto", "Silencio"},
    {"Run in background", "Jouer en arrière-plan", "Im Hintergrund weiter", "Gioca in background", "Seguir en segundo plano"},
    {"Fast forward speed", "Vitesse d'avance rapide", "Schnelllauf-Tempo", "Velocità avanti veloce", "Velocidad de avance rápido"},
    {"Maximum", "Maximum", "Maximal", "Massima", "Máxima"},
    {"Fast forward key", "Touche d'avance rapide", "Schnelllauf-Taste", "Tasto avanti veloce", "Botón de avance rápido"},
    {"Hold", "Maintenir", "Halten", "Tieni premuto", "Mantener"},
    {"Press on / off", "Appui marche / arrêt", "Drücken an / aus", "Premi on / off", "Pulsar sí / no"},
    {"Fast boot", "Démarrage rapide", "Schnellstart", "Avvio rapido", "Inicio rápido"},
    {"Hide the idle pointer", "Cacher le pointeur inactif", "Ruhenden Zeiger ausblenden", "Nascondi il puntatore fermo", "Ocultar el puntero quieto"},
    {"Cursor speed", "Vitesse du curseur", "Cursor-Tempo", "Velocità cursore", "Velocidad del cursor"},
    {"Swap the sticks", "Inverser les sticks", "Sticks tauschen", "Inverti le levette", "Intercambiar sticks"},
    // The picture filters
    {"Sharp", "Net", "Scharf", "Nitido", "Nítido"},
    {"Smooth", "Lisse", "Weich", "Morbido", "Suave"},
    {"Pixels", "Pixels", "Pixel", "Pixel", "Píxeles"},
    // The rows' notes
    {"3D at 1x and 30 fps, whatever the looks above say: lighter on the battery and the fans.",
     "3D en 1x et 30 i/s, quels que soient les réglages au-dessus : moins de batterie et de ventilation.",
     "3D in 1x und 30 fps, egal was oben steht: schont Akku und Lüfter.",
     "3D a 1x e 30 fps, qualunque cosa dicano le opzioni sopra: meno batteria e ventole.",
     "3D a 1x y 30 fps, digan lo que digan los ajustes de arriba: menos batería y ventilador."},
    {"Off: the game pauses and goes quiet while its window is not in front.",
     "Non : le jeu se met en pause et se tait quand sa fenêtre n'est pas devant.",
     "Aus: das Spiel pausiert und schweigt, solange sein Fenster nicht vorne ist.",
     "No: il gioco va in pausa e tace quando la sua finestra non è in primo piano.",
     "No: el juego se pausa y calla mientras su ventana no está delante."},
    {"The boot's splashes, which wait for no button, go by at full speed.",
     "Les écrans du démarrage, qui n'attendent aucun bouton, défilent à pleine vitesse.",
     "Die Startbildschirme, die auf keine Taste warten, laufen mit voller Geschwindigkeit.",
     "Le schermate d'avvio, che non aspettano alcun tasto, scorrono a piena velocità.",
     "Las pantallas de inicio, que no esperan ningún botón, pasan a toda velocidad."},
    {"Sharp: crisp pixels, evenly sized. Smooth: softer. Pixels: the DS's raw pixels.",
     "Net : des pixels nets et réguliers. Lisse : plus doux. Pixels : les pixels bruts de la DS.",
     "Scharf: klare, gleich große Pixel. Weich: sanfter. Pixel: die rohen Pixel der DS.",
     "Nitido: pixel netti e regolari. Morbido: più sfumato. Pixel: i pixel grezzi del DS.",
     "Nítido: píxeles limpios y regulares. Suave: más difuminado. Píxeles: los píxeles en bruto de la DS."},
    // The ROM's folder cannot be written: the save in the settings folder (the pause menu's banner)
    {"The ROM's folder is read-only: the game saves to @1", "Le dossier de la ROM est en lecture seule : le jeu sauvegarde dans @1",
     "Der ROM-Ordner ist schreibgeschützt: das Spiel speichert in @1", "La cartella della ROM è di sola lettura: il gioco salva in @1",
     "La carpeta de la ROM es de solo lectura: el juego guarda en @1"},
    // A screenshot of the window
    {"Screenshot saved: @1", "Capture enregistrée : @1", "Bildschirmfoto gespeichert: @1", "Screenshot salvato: @1", "Captura guardada: @1"},
    // The ROM's folder writable again: the save back beside it
    {"The game saves beside the ROM again: @1", "Le jeu sauvegarde de nouveau à côté de la ROM : @1",
     "Das Spiel speichert wieder neben dem ROM: @1", "Il gioco salva di nuovo accanto alla ROM: @1",
     "El juego vuelve a guardar junto a la ROM: @1"},
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
