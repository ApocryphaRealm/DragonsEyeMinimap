# -*- coding: utf-8 -*-
"""gen-translations.py - builds the eleven DragonsEyeMinimap_<language>.txt files.

The English key list is extracted from the PATCHED source/UI.cpp, so it can never drift from the
code. Two shapes are read:

  * strings::TR("KEY", "English text") - the ordinary call;
  * a parallel `kFooKeys[] / kFooLabels[]` array pair - the shape and corner combos, the log
    levels, and the reasons a key cannot be bound, whose entries are looked up by index at draw
    time rather than by a literal TR call.

The other ten languages are this project's own translations of that list, held below as one dict
per key. Writes REPO/dist/Interface/Translations/DragonsEyeMinimap_<language>.txt for english +
the ten languages: UTF-16LE with a BOM, one "$key<TAB>text" record per line, a literal "\\n" for an
embedded line break, CRLF records - the SKSE/SkyUI shape the Apocrypha Menu Framework's
Strings.cpp reads.

Untranslated on purpose, in every language: product and mod names (Skyrim, Dragon's Eye Minimap,
Local Map Upgrade, Apocrypha Menu Framework, Untarnished UI, SkyUI); folder and file names
(SWF, INI); the XInput button names
and masks (R3, L3, LB, RB, Start, Back, A, B, X, Y); the log-level values as the INI writes them
where a language has no established word for them; and every printf specifier, kept in the same
order as the English.

Run: `python tools/gen-translations.py`.
"""
import io
import os
import re

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STEM = "DragonsEyeMinimap"
LANGS = ["english", "japanese", "korean", "chinese", "russian",
         "german", "french", "spanish", "italian", "polish", "czech"]

TR_RE = re.compile(r'strings::TR\(\s*"((?:[^"\\]|\\.)+)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*\)')
KEYS_RE = re.compile(r'constexpr const char\* const k(\w+)Keys\[\]\s*=\s*\{([^}]*)\};')
LABELS_RE = re.compile(r'constexpr const char\* const k(\w+)Labels\[\]\s*=\s*\{([^}]*)\};')
LIT_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')


def unescape(s):
    return s.encode("latin-1", "backslashreplace").decode("unicode_escape") if "\\" in s else s


def read_keys():
    src = io.open(os.path.join(REPO, "source", "UI.cpp"), encoding="utf-8", newline="").read()
    order, texts = [], {}

    def add(key, text):
        if key in texts:
            if texts[key] != text:
                raise RuntimeError("key {!r} has two English texts: {!r} vs {!r}".format(key, texts[key], text))
            return
        order.append(key)
        texts[key] = text

    for m in TR_RE.finditer(src):
        add(unescape(m.group(1)), unescape(m.group(2)))

    labels = {m.group(1): m.group(2) for m in LABELS_RE.finditer(src)}
    for m in KEYS_RE.finditer(src):
        name = m.group(1)
        if name not in labels:
            raise RuntimeError("k{}Keys has no matching k{}Labels array".format(name, name))
        ks = [unescape(x) for x in LIT_RE.findall(m.group(2))]
        vs = [unescape(x) for x in LIT_RE.findall(labels[name])]
        if len(ks) != len(vs):
            raise RuntimeError("k{}Keys ({}) and k{}Labels ({}) length mismatch".format(name, len(ks), name, len(vs)))
        for k, v in zip(ks, vs):
            add(k, v)

    return order, texts


# ------------------------------------------------------------------------------------------------
# The project's own translations, one dict per key, in the order the page draws them.
# ------------------------------------------------------------------------------------------------
def T(ja, ko, zh, ru, de, fr, es, it, pl, cs):
    return {"japanese": ja, "korean": ko, "chinese": zh, "russian": ru, "german": de,
            "french": fr, "spanish": es, "italian": it, "polish": pl, "czech": cs}


TRANSLATIONS = {}

# --- the two bind-refusal status lines and the key-role words they substitute -------------------
TRANSLATIONS["DEM_StatusKeyReserved"] = T(
    "そのキーは予約されています (%s)。別のキーを押してください。",
    "해당 키는 예약되어 있습니다 (%s). 다른 키를 누르세요.",
    "该按键已被占用 (%s)。请按其他按键。",
    "Эта клавиша зарезервирована (%s). Нажмите другую клавишу.",
    "Diese Taste ist reserviert (%s). Drücke eine andere Taste.",
    "Cette touche est réservée (%s). Appuyez sur une autre touche.",
    "Esa tecla está reservada (%s). Pulsa otra tecla.",
    "Quel tasto è riservato (%s). Premi un altro tasto.",
    "Ten klawisz jest zarezerwowany (%s). Naciśnij inny klawisz.",
    "Tato klávesa je vyhrazena (%s). Stiskni jinou klávesu.")
TRANSLATIONS["DEM_KeyRoleZoomToggle"] = T(
    "ズーム切り替え", "확대 전환", "缩放切换", "переключения масштаба", "Zoomumschaltung",
    "de basculement du zoom", "de cambio de zoom", "di cambio zoom", "przełączania powiększenia", "přepínání přiblížení")
TRANSLATIONS["DEM_KeyRoleHide"] = T(
    "非表示", "숨기기", "隐藏", "скрытия", "Ausblenden",
    "de masquage", "de ocultar", "per nascondere", "ukrywania", "skrytí")
TRANSLATIONS["DEM_StatusKeyTaken"] = T(
    "そのキーはすでに%sキーです。別のキーを押してください。",
    "해당 키는 이미 %s 키입니다. 다른 키를 누르세요.",
    "该按键已经是%s按键。请按其他按键。",
    "Эта клавиша уже назначена для %s. Нажмите другую клавишу.",
    "Diese Taste ist bereits die Taste für %s. Drücke eine andere Taste.",
    "Cette touche est déjà la touche %s. Appuyez sur une autre touche.",
    "Esa tecla ya es la tecla %s. Pulsa otra tecla.",
    "Quel tasto è già il tasto %s. Premi un altro tasto.",
    "Ten klawisz jest już klawiszem %s. Naciśnij inny klawisz.",
    "Tato klávesa už je klávesa %s. Stiskni jinou klávesu.")

# --- the bind row -------------------------------------------------------------------------------
TRANSLATIONS["DEM_PressAKey"] = T(
    "キーを押してください…（キャンセル）", "키를 누르세요... (취소)", "请按一个键…（取消）",
    "Нажмите клавишу... (отмена)", "Taste drücken ... (abbrechen)",
    "Appuyez sur une touche... (annuler)", "Pulsa una tecla... (cancelar)",
    "Premi un tasto... (annulla)", "Naciśnij klawisz... (anuluj)", "Stiskni klávesu... (zrušit)")
TRANSLATIONS["DEM_Bind"] = T(
    "割り当て", "지정", "绑定", "Назначить", "Belegen",
    "Assigner", "Asignar", "Assegna", "Przypisz", "Přiřadit")

# --- Display ------------------------------------------------------------------------------------
TRANSLATIONS["DEM_SecDisplay"] = T(
    "表示", "표시", "显示", "Отображение", "Anzeige",
    "Affichage", "Visualización", "Visualizzazione", "Wyświetlanie", "Zobrazení")
TRANSLATIONS["DEM_Corner"] = T(
    "配置コーナー", "모서리", "屏幕角落", "Угол", "Ecke",
    "Coin", "Esquina", "Angolo", "Róg", "Roh")
TRANSLATIONS["DEM_HelpCorner"] = T(
    "ミニマップを置く画面の隅です。オフセットが両方とも 0 のとき、画像はその隅にぴったり揃います。",
    "미니맵이 놓이는 화면 모서리입니다. 두 오프셋이 모두 0이면 아트가 그 모서리에 딱 맞춰집니다.",
    "小地图所在的屏幕角落。两个偏移都为 0 时，图形会与该角落齐平对齐。",
    "В каком углу экрана находится миникарта. Когда оба смещения равны 0, графика прилегает вплотную к этому углу.",
    "In welcher Bildschirmecke die Minikarte sitzt. Stehen beide Versätze auf 0, schließt die Grafik bündig mit dieser Ecke ab.",
    "Le coin de l'écran où se place la minicarte. Avec les deux décalages à 0, le graphisme s'aligne exactement sur ce coin.",
    "En qué esquina de la pantalla se sitúa el minimapa. Con ambos desplazamientos a 0, el gráfico queda alineado a ras de esa esquina.",
    "In quale angolo dello schermo sta la minimappa. Con entrambi gli scostamenti a 0 la grafica si allinea a filo con quell'angolo.",
    "W którym rogu ekranu znajduje się minimapa. Przy obu przesunięciach równych 0 grafika przylega równo do tego rogu.",
    "V kterém rohu obrazovky je minimapa. Když jsou oba posuny 0, grafika lícuje přesně s tímto rohem.")
TRANSLATIONS["DEM_OffsetX"] = T(
    "オフセット X", "오프셋 X", "偏移 X", "Смещение X", "Versatz X",
    "Décalage X", "Desplazamiento X", "Scostamento X", "Przesunięcie X", "Posun X")
TRANSLATIONS["DEM_HelpOffsetX"] = T(
    "隅からのずらし量（画面ピクセル）。どの隅に固定していても、正の値は常に右方向です。隅ごとに専用の一組を記憶します。",
    "모서리로부터의 이동량(화면 픽셀). 어느 모서리에 고정하든 양수는 항상 오른쪽입니다. 모서리마다 자체 값 한 쌍을 기억합니다.",
    "相对角落的微调量，单位为屏幕像素。无论固定在哪个角落，正值始终向右。每个角落各自记住一组数值。",
    "Сдвиг от угла в пикселях экрана. Положительное значение всегда вправо, к какому бы углу ни была привязана карта. Для каждого угла запоминается своя пара.",
    "Verschiebung von der Ecke, in Bildschirmpixeln. Positiv ist immer nach rechts, egal welche Ecke verankert ist. Jede Ecke merkt sich ihr eigenes Paar.",
    "Décalage depuis le coin, en pixels d'écran. Le positif va toujours vers la droite, quel que soit le coin ancré. Chaque coin retient sa propre paire.",
    "Desplazamiento desde la esquina, en píxeles de pantalla. Lo positivo va siempre hacia la derecha, sea cual sea la esquina anclada. Cada esquina recuerda su propio par.",
    "Scostamento dall'angolo, in pixel dello schermo. Il positivo va sempre verso destra, qualunque sia l'angolo ancorato. Ogni angolo ricorda la propria coppia.",
    "Przesunięcie od rogu, w pikselach ekranu. Wartość dodatnia zawsze w prawo, niezależnie od zakotwiczonego rogu. Każdy róg pamięta własną parę.",
    "Posun od rohu, v pixelech obrazovky. Kladná hodnota je vždy doprava, ať je ukotven kterýkoli roh. Každý roh si pamatuje vlastní dvojici.")
TRANSLATIONS["DEM_OffsetY"] = T(
    "オフセット Y", "오프셋 Y", "偏移 Y", "Смещение Y", "Versatz Y",
    "Décalage Y", "Desplazamiento Y", "Scostamento Y", "Przesunięcie Y", "Posun Y")
TRANSLATIONS["DEM_HelpOffsetY"] = T(
    "隅からのずらし量（画面ピクセル）。どの隅に固定していても、正の値は常に下方向です。隅ごとに専用の一組を記憶します。",
    "모서리로부터의 이동량(화면 픽셀). 어느 모서리에 고정하든 양수는 항상 아래쪽입니다. 모서리마다 자체 값 한 쌍을 기억합니다.",
    "相对角落的微调量，单位为屏幕像素。无论固定在哪个角落，正值始终向下。每个角落各自记住一组数值。",
    "Сдвиг от угла в пикселях экрана. Положительное значение всегда вниз, к какому бы углу ни была привязана карта. Для каждого угла запоминается своя пара.",
    "Verschiebung von der Ecke, in Bildschirmpixeln. Positiv ist immer nach unten, egal welche Ecke verankert ist. Jede Ecke merkt sich ihr eigenes Paar.",
    "Décalage depuis le coin, en pixels d'écran. Le positif va toujours vers le bas, quel que soit le coin ancré. Chaque coin retient sa propre paire.",
    "Desplazamiento desde la esquina, en píxeles de pantalla. Lo positivo va siempre hacia abajo, sea cual sea la esquina anclada. Cada esquina recuerda su propio par.",
    "Scostamento dall'angolo, in pixel dello schermo. Il positivo va sempre verso il basso, qualunque sia l'angolo ancorato. Ogni angolo ricorda la propria coppia.",
    "Przesunięcie od rogu, w pikselach ekranu. Wartość dodatnia zawsze w dół, niezależnie od zakotwiczonego rogu. Każdy róg pamięta własną parę.",
    "Posun od rohu, v pixelech obrazovky. Kladná hodnota je vždy dolů, ať je ukotven kterýkoli roh. Každý roh si pamatuje vlastní dvojici.")
TRANSLATIONS["DEM_EditingOffset"] = T(
    "%s のオフセットを編集しています。",
    "%s 오프셋을 편집 중입니다.",
    "正在编辑%s的偏移。",
    "Редактируется смещение для угла: %s.",
    "Bearbeitet wird der Versatz für %s.",
    "Modification du décalage pour %s.",
    "Editando el desplazamiento de %s.",
    "Modifica dello scostamento per %s.",
    "Edytowane jest przesunięcie dla %s.",
    "Upravuje se posun pro %s.")
TRANSLATIONS["DEM_Scale"] = T(
    "サイズ", "크기", "缩放", "Масштаб", "Größe",
    "Échelle", "Escala", "Scala", "Skala", "Měřítko")
TRANSLATIONS["DEM_HelpScale"] = T(
    "ミニマップの大きさ。1.00 は画像が描かれたそのままの大きさです。上限は、ミニマップが画面の 4 分の 1 に収まるよう制限されています。",
    "미니맵의 크기입니다. 1.00은 아트가 그려진 원래 크기입니다. 미니맵이 화면의 4분의 1 안에 들어가도록 상한이 제한됩니다.",
    "小地图的大小。1.00 即图形绘制时的原始尺寸。上限受到限制，以保证小地图不超过屏幕的四分之一。",
    "Размер миникарты. 1.00 - размер, в котором нарисована графика. Верхняя граница ограничена так, чтобы миникарта умещалась в четверть экрана.",
    "Größe der Minikarte. 1.00 ist die Größe, in der die Grafik gezeichnet wurde. Das obere Ende ist gedeckelt, damit die Minikarte innerhalb eines Viertels des Bildschirms bleibt.",
    "Taille de la minicarte. 1.00 correspond à la taille à laquelle le graphisme a été dessiné. Le haut de la plage est plafonné pour que la minicarte reste dans un quart de l'écran.",
    "Tamaño del minimapa. 1.00 es el tamaño al que se dibujó el gráfico. El extremo superior está limitado para que el minimapa quepa en un cuarto de la pantalla.",
    "Dimensione della minimappa. 1.00 è la dimensione con cui è stata disegnata la grafica. L'estremo superiore è limitato perché la minimappa resti entro un quarto dello schermo.",
    "Rozmiar minimapy. 1.00 to rozmiar, w jakim narysowano grafikę. Górna granica jest ograniczona, aby minimapa mieściła się w jednej czwartej ekranu.",
    "Velikost minimapy. 1.00 je velikost, v jaké byla grafika nakreslena. Horní mez je omezena tak, aby se minimapa vešla do čtvrtiny obrazovky.")
TRANSLATIONS["DEM_LargestAllowed"] = T(
    "上限: %.2f（画面の 4 分の 1）",
    "최대 허용: %.2f (화면의 4분의 1)",
    "允许的最大值：%.2f（屏幕的四分之一）",
    "Максимум: %.2f (четверть экрана)",
    "Größter erlaubter Wert: %.2f (ein Viertel des Bildschirms)",
    "Maximum autorisé : %.2f (un quart de l'écran)",
    "Máximo permitido: %.2f (un cuarto de la pantalla)",
    "Massimo consentito: %.2f (un quarto dello schermo)",
    "Największa dozwolona: %.2f (jedna czwarta ekranu)",
    "Největší povolená: %.2f (čtvrtina obrazovky)")
TRANSLATIONS["DEM_Shape"] = T(
    "形状", "모양", "形状", "Форма", "Form",
    "Forme", "Forma", "Forma", "Kształt", "Tvar")
TRANSLATIONS["DEM_HelpShape"] = T(
    "ミニマップを四角で描くか、円で描くか。",
    "미니맵을 사각형으로 그릴지 원형으로 그릴지 정합니다.",
    "小地图绘制为方形还是圆形。",
    "Рисуется ли миникарта квадратом или кругом.",
    "Ob die Minikarte als Quadrat oder als Kreis gezeichnet wird.",
    "Si la minicarte est dessinée en carré ou en cercle.",
    "Si el minimapa se dibuja como cuadrado o como círculo.",
    "Se la minimappa è disegnata come quadrato o come cerchio.",
    "Czy minimapa jest rysowana jako kwadrat, czy jako koło.",
    "Zda se minimapa kreslí jako čtverec, nebo jako kruh.")
TRANSLATIONS["DEM_ShowMinimap"] = T(
    "ミニマップを表示", "미니맵 표시", "显示小地图", "Показывать миникарту", "Minikarte anzeigen",
    "Afficher la minicarte", "Mostrar el minimapa", "Mostra la minimappa", "Pokaż minimapę", "Zobrazit minimapu")
TRANSLATIONS["DEM_HelpShowMinimap"] = T(
    "ミニマップを今すぐ表示または非表示にし、次回プレイ時のためにその選択を記憶します。",
    "지금 바로 미니맵을 숨기거나 표시하며, 다음에 플레이할 때를 위해 선택을 기억합니다.",
    "立即隐藏或显示小地图，并记住该选择供下次游戏使用。",
    "Скрывает или показывает миникарту прямо сейчас и запоминает выбор до следующей игры.",
    "Blendet die Minikarte sofort aus oder ein und merkt sich die Wahl für das nächste Spiel.",
    "Masque ou affiche la minicarte immédiatement, et retient ce choix pour la prochaine partie.",
    "Oculta o muestra el minimapa ahora mismo y recuerda la elección para la próxima vez que juegues.",
    "Nasconde o mostra la minimappa subito e ricorda la scelta per la prossima partita.",
    "Natychmiast ukrywa lub pokazuje minimapę i zapamiętuje wybór na następną grę.",
    "Okamžitě skryje nebo zobrazí minimapu a zapamatuje si volbu na příště.")
TRANSLATIONS["DEM_ShowOnStart"] = T(
    "ゲーム開始時にミニマップを表示", "게임 시작 시 미니맵 표시", "游戏开始时显示小地图",
    "Показывать миникарту при запуске игры", "Minikarte beim Spielstart anzeigen",
    "Afficher la minicarte au démarrage du jeu", "Mostrar el minimapa al iniciar el juego",
    "Mostra la minimappa all'avvio del gioco", "Pokaż minimapę przy starcie gry", "Zobrazit minimapu při spuštění hry")
TRANSLATIONS["DEM_HelpShowOnStart"] = T(
    "ミニマップはまだ作成されていないため、これは作成された後の挙動を決めるだけです。",
    "미니맵이 아직 만들어지지 않았으므로, 이 설정은 만들어진 뒤의 동작만 정합니다.",
    "小地图尚未创建，因此该项仅决定创建之后的行为。",
    "Миникарта ещё не создана, поэтому это лишь задаёт, что произойдёт, когда она появится.",
    "Die Minikarte wurde noch nicht erstellt, daher legt dies nur fest, was geschieht, sobald sie es ist.",
    "La minicarte n'a pas encore été créée ; ceci ne fait que définir ce qui se passera une fois qu'elle le sera.",
    "El minimapa aún no se ha creado, así que esto solo define qué ocurrirá cuando lo esté.",
    "La minimappa non è ancora stata creata, quindi questo definisce solo cosa accadrà una volta creata.",
    "Minimapa nie została jeszcze utworzona, więc to ustawia tylko, co się stanie, gdy już powstanie.",
    "Minimapa zatím nebyla vytvořena, takže tohle jen určuje, co se stane, až vytvořena bude.")

# --- Map zoom -----------------------------------------------------------------------------------
TRANSLATIONS["DEM_SecMapZoom"] = T(
    "マップのズーム", "지도 확대", "地图缩放", "Масштаб карты", "Kartenzoom",
    "Zoom de la carte", "Zoom del mapa", "Zoom della mappa", "Powiększenie mapy", "Přiblížení mapy")
TRANSLATIONS["DEM_ZoomToggleKey"] = T(
    "ズーム切り替えキー", "확대 전환 키", "缩放切换按键", "Клавиша переключения масштаба", "Taste für Zoomumschaltung",
    "Touche de basculement du zoom", "Tecla de cambio de zoom", "Tasto di cambio zoom",
    "Klawisz przełączania powiększenia", "Klávesa přepínání přiblížení")
TRANSLATIONS["DEM_HelpZoomToggleKey"] = T(
    "コントロールキーを押しながらスクロールする代わりに、このキーで下の 2 つのズーム倍率を切り替えます。0 で無効になります。",
    "컨트롤 키를 누른 채 스크롤하는 대신, 이 키로 아래 두 확대 단계를 오갑니다. 0이면 사용하지 않습니다.",
    "按此键可在下面两个缩放级别之间切换，无需按住控制键滚动。设为 0 则禁用。",
    "Нажмите эту клавишу, чтобы переключаться между двумя уровнями масштаба ниже, вместо удержания клавиши управления с прокруткой. 0 отключает её.",
    "Mit dieser Taste springst du zwischen den beiden Zoomstufen unten, statt die Steuerungstaste zu halten und zu scrollen. 0 schaltet sie ab.",
    "Appuyez sur cette touche pour passer d'un des deux niveaux de zoom ci-dessous à l'autre, au lieu de maintenir la touche de contrôle et de faire défiler. 0 la désactive.",
    "Pulsa esta tecla para saltar entre los dos niveles de zoom de abajo, en lugar de mantener la tecla de control y desplazar la rueda. 0 la desactiva.",
    "Premi questo tasto per passare tra i due livelli di zoom qui sotto, invece di tenere premuto il tasto di controllo e scorrere. 0 lo disattiva.",
    "Naciśnij ten klawisz, aby przeskakiwać między dwoma poziomami powiększenia poniżej, zamiast trzymać klawisz sterowania i przewijać. 0 go wyłącza.",
    "Touto klávesou přepínáš mezi dvěma úrovněmi přiblížení níže, místo držení ovládací klávesy a rolování. 0 ji vypne.")
TRANSLATIONS["DEM_NoZoomKey"] = T(
    "ズームキーは設定されていません。", "확대 키가 설정되지 않았습니다.", "未设置缩放按键。",
    "Клавиша масштаба не назначена.", "Keine Zoomtaste festgelegt.",
    "Aucune touche de zoom définie.", "No hay tecla de zoom definida.",
    "Nessun tasto di zoom impostato.", "Nie ustawiono klawisza powiększenia.", "Není nastavena klávesa přiblížení.")
TRANSLATIONS["DEM_DefaultZoom"] = T(
    "既定のズーム", "기본 확대", "默认缩放", "Обычный масштаб", "Standardzoom",
    "Zoom par défaut", "Zoom predeterminado", "Zoom predefinito", "Domyślne powiększenie", "Výchozí přiblížení")
TRANSLATIONS["DEM_SetToCurrent"] = T(
    "現在の値にする", "현재 값으로 설정", "设为当前值", "Взять текущее", "Auf aktuellen Wert setzen",
    "Utiliser la valeur actuelle", "Usar el valor actual", "Imposta al valore attuale",
    "Ustaw na bieżącą", "Nastavit na aktuální")
TRANSLATIONS["DEM_ZoomedIn"] = T(
    "拡大時のズーム", "확대 시", "放大后", "Приближённый масштаб", "Herangezoomt",
    "Zoom rapproché", "Zoom acercado", "Zoom ravvicinato", "Przybliżone", "Přiblížené")
TRANSLATIONS["DEM_HelpZoomLevels"] = T(
    "ズーム切り替えキーはこの 2 つを交互に切り替えます。マップを好みの倍率にしてから「現在の値にする」を押すと、ゲームが説明していない単位の数値を入力せずにその倍率を保存できます。",
    "확대 전환 키는 이 둘을 번갈아 사용합니다. 지도를 원하는 만큼 확대한 뒤 \"현재 값으로 설정\"을 누르면, 게임이 설명하지 않는 단위의 숫자를 입력하지 않고 그 단계를 저장할 수 있습니다.",
    "缩放切换按键会在这两个值之间来回切换。先把地图缩放到想要的程度，再按“设为当前值”即可保存该级别，而不必输入游戏未作说明的单位数值。",
    "Клавиша переключения масштаба чередует эти два значения. Приблизьте карту так, как хотите, затем нажмите «Взять текущее», чтобы сохранить этот уровень, а не вводить число в единицах, которые игра нигде не описывает.",
    "Die Zoomumschalttaste wechselt zwischen diesen beiden. Zoome die Karte so, wie du sie haben willst, und drücke dann \"Auf aktuellen Wert setzen\", statt eine Zahl in Einheiten einzutippen, die das Spiel nirgends dokumentiert.",
    "La touche de basculement du zoom alterne entre ces deux valeurs. Zoomez la carte comme vous le souhaitez, puis appuyez sur \"Utiliser la valeur actuelle\" pour enregistrer ce niveau plutôt que de saisir un nombre dans des unités que le jeu ne documente pas.",
    "La tecla de cambio de zoom alterna entre estos dos valores. Ajusta el zoom del mapa como quieras y pulsa \"Usar el valor actual\" para guardar ese nivel en vez de teclear un número en unidades que el juego no documenta.",
    "Il tasto di cambio zoom alterna questi due valori. Porta la mappa allo zoom che preferisci, poi premi \"Imposta al valore attuale\" per salvare quel livello invece di digitare un numero in unità che il gioco non documenta.",
    "Klawisz przełączania powiększenia przeskakuje między tymi dwiema wartościami. Ustaw mapę tak, jak chcesz, a potem naciśnij \"Ustaw na bieżącą\", zamiast wpisywać liczbę w jednostkach, których gra nie opisuje.",
    "Klávesa přepínání přiblížení střídá tyto dvě hodnoty. Přibliž mapu, jak chceš, a pak stiskni \"Nastavit na aktuální\" místo psaní čísla v jednotkách, které hra nikde nepopisuje.")
TRANSLATIONS["DEM_ZoomNotReady"] = T(
    "ミニマップが動き出したら、ゲーム内でマップをズームして「現在の値にする」を使ってください。上の数値はカメラ独自の単位で、説明されていません。",
    "미니맵이 실행된 뒤 게임 안에서 지도를 확대하고 \"현재 값으로 설정\"을 사용하세요. 위의 숫자는 카메라 고유 단위이며 문서화되어 있지 않습니다.",
    "待小地图运行后，在游戏中缩放地图并使用“设为当前值”。上面的数值使用摄像机自身的单位，并无说明文档。",
    "Приблизьте карту в игре и воспользуйтесь «Взять текущее», когда миникарта заработает - числа выше даны в собственных единицах камеры, которые нигде не описаны.",
    "Zoome die Karte im Spiel und nutze \"Auf aktuellen Wert setzen\", sobald die Minikarte läuft - die Zahlen oben stehen in den eigenen Einheiten der Kamera, die nirgends dokumentiert sind.",
    "Zoomez la carte en jeu et utilisez \"Utiliser la valeur actuelle\" une fois la minicarte en fonctionnement - les nombres ci-dessus sont exprimés dans les unités propres de la caméra, qui ne sont pas documentées.",
    "Ajusta el zoom del mapa en el juego y usa \"Usar el valor actual\" cuando el minimapa esté funcionando: los números de arriba están en las unidades propias de la cámara, que no están documentadas.",
    "Zooma la mappa in gioco e usa \"Imposta al valore attuale\" quando la minimappa è in funzione: i numeri qui sopra sono nelle unità proprie della telecamera, che non sono documentate.",
    "Przybliż mapę w grze i użyj \"Ustaw na bieżącą\", gdy minimapa już działa - liczby powyżej są w jednostkach własnych kamery, których nikt nie opisał.",
    "Přibliž mapu ve hře a použij \"Nastavit na aktuální\", jakmile minimapa běží - čísla výše jsou ve vlastních jednotkách kamery, které nejsou nikde popsány.")
TRANSLATIONS["DEM_LiveZoom"] = T(
    "現在のズーム", "실시간 확대", "实时缩放", "Текущий масштаб", "Aktueller Zoom",
    "Zoom en direct", "Zoom actual", "Zoom attuale", "Bieżące powiększenie", "Aktuální přiblížení")
TRANSLATIONS["DEM_HelpLiveZoom"] = T(
    "現在ミニマップがどれだけ拡大されているか。ゲーム側の制限が働くため、設定した位置とは違う値に落ち着くことがあります。",
    "지금 미니맵이 얼마나 확대되어 있는지 나타냅니다. 게임이 자체 한계를 적용하므로 값이 지정한 곳과 다른 자리에 머무를 수 있습니다.",
    "小地图当前的放大程度。游戏会施加自身限制，因此数值可能停在与你设定不同的位置。",
    "Насколько миникарта приближена прямо сейчас. Игра накладывает собственные ограничения, поэтому значение может остановиться не там, где вы его оставили.",
    "Wie weit die Minikarte gerade herangezoomt ist. Das Spiel setzt eigene Grenzen, daher kann der Wert woanders landen als dort, wo du ihn gelassen hast.",
    "Le niveau de zoom actuel de la minicarte. Le jeu applique ses propres limites, si bien que la valeur peut se fixer ailleurs que là où vous l'avez laissée.",
    "Cuánto está acercado el minimapa ahora mismo. El juego aplica sus propios límites, así que el valor puede quedarse en un punto distinto del que dejaste.",
    "Quanto è ingrandita la minimappa in questo momento. Il gioco applica i propri limiti, quindi il valore può fermarsi altrove rispetto a dove l'hai lasciato.",
    "Jak bardzo minimapa jest teraz przybliżona. Gra narzuca własne ograniczenia, więc wartość może osiąść gdzie indziej, niż ją zostawiono.",
    "Jak moc je minimapa právě přiblížená. Hra uplatňuje vlastní meze, takže hodnota se může ustálit jinde, než kde jsi ji nechal.")

# --- Compass ------------------------------------------------------------------------------------
TRANSLATIONS["DEM_SecCompass"] = T(
    "コンパス", "나침반", "指南针", "Компас", "Kompass",
    "Boussole", "Brújula", "Bussola", "Kompas", "Kompas")
TRANSLATIONS["DEM_CompassRing"] = T(
    "コンパスリング", "나침반 링", "指南针环", "Кольцо компаса", "Kompassring",
    "Anneau de boussole", "Anillo de brújula", "Anello della bussola", "Pierścień kompasu", "Kompasový kruh")
TRANSLATIONS["DEM_HelpCompassRing"] = T(
    "マップを非表示にしている間、ミニマップの隅を引き継ぐコンパスリングです。オフにすると、マップ非表示時にそこには何も描かれません。",
    "지도를 숨긴 동안 미니맵의 모서리를 차지하는 나침반 링입니다. 끄면 지도를 숨겼을 때 그 자리에 아무것도 그려지지 않습니다.",
    "地图隐藏时占据小地图角落位置的指南针环。关闭后，地图隐藏时该处不绘制任何内容。",
    "Кольцо компаса, занимающее угол миникарты, пока карта скрыта. Выключено - при скрытой карте там ничего не рисуется.",
    "Der Kompassring, der die Ecke der Minikarte einnimmt, solange die Karte ausgeblendet ist. Aus = bei ausgeblendeter Karte wird dort nichts gezeichnet.",
    "L'anneau de boussole qui occupe le coin de la minicarte lorsque la carte est masquée. Désactivé = rien n'y est dessiné quand la carte est masquée.",
    "El anillo de brújula que ocupa la esquina del minimapa mientras el mapa está oculto. Desactivado = no se dibuja nada ahí cuando el mapa está oculto.",
    "L'anello della bussola che occupa l'angolo della minimappa mentre la mappa è nascosta. Disattivato = non viene disegnato nulla lì quando la mappa è nascosta.",
    "Pierścień kompasu, który zajmuje róg minimapy, gdy mapa jest ukryta. Wyłączone = przy ukrytej mapie nic tam nie jest rysowane.",
    "Kompasový kruh, který zabírá roh minimapy, když je mapa skrytá. Vypnuto = při skryté mapě se tam nekreslí nic.")
TRANSLATIONS["DEM_QuestPointer"] = T(
    "クエストポインター", "퀘스트 표시기", "任务指针", "Указатель задания", "Questzeiger",
    "Indicateur de quête", "Indicador de misión", "Indicatore di missione", "Wskaźnik zadania", "Ukazatel úkolu")
TRANSLATIONS["DEM_HelpQuestPointer"] = T(
    "距離表示付きのバニラ風クエストマーカーで、リングまたは表示中のマップに乗ります。オフにすると一切描かれません。",
    "거리 표시가 있는 바닐라 스타일 퀘스트 마커로, 링이나 표시 중인 지도 위에 놓입니다. 끄면 전혀 그려지지 않습니다.",
    "带距离读数的原版风格任务标记，附着在指南针环或显示中的地图上。关闭后将完全不绘制。",
    "Маркер задания в стиле оригинала с показом расстояния, размещённый на кольце или на видимой карте. Выключено - не рисуется никогда.",
    "Der Questmarker im Vanilla-Stil mit Entfernungsanzeige, der auf dem Ring oder der sichtbaren Karte sitzt. Aus = wird nie gezeichnet.",
    "Le marqueur de quête de style vanilla avec l'affichage de la distance, posé sur l'anneau ou sur la carte visible. Désactivé = jamais dessiné.",
    "El marcador de misión de estilo vanilla con la lectura de distancia, montado en el anillo o en el mapa visible. Desactivado = no se dibuja nunca.",
    "L'indicatore di missione in stile vanilla con la lettura della distanza, posto sull'anello o sulla mappa visibile. Disattivato = non viene mai disegnato.",
    "Znacznik zadania w stylu podstawowej gry z odczytem odległości, umieszczony na pierścieniu lub na widocznej mapie. Wyłączone = nigdy nie rysowany.",
    "Značka úkolu ve stylu základní hry s údajem o vzdálenosti, umístěná na kruhu nebo na viditelné mapě. Vypnuto = nekreslí se nikdy.")
TRANSLATIONS["DEM_MetricUnits"] = T(
    "メートル法", "미터법 단위", "公制单位", "Метрические единицы", "Metrische Einheiten",
    "Unités métriques", "Unidades métricas", "Unità metriche", "Jednostki metryczne", "Metrické jednotky")
TRANSLATIONS["DEM_HelpMetricUnits"] = T(
    "距離表示をフィートではなくメートルにします。",
    "거리 표시를 피트 대신 미터로 표시합니다.",
    "距离读数使用米而非英尺。",
    "Показ расстояния в метрах вместо футов.",
    "Entfernungsanzeige in Metern statt in Fuß.",
    "Affichage de la distance en mètres plutôt qu'en pieds.",
    "Lectura de distancia en metros en lugar de pies.",
    "Lettura della distanza in metri invece che in piedi.",
    "Odczyt odległości w metrach zamiast w stopach.",
    "Údaj o vzdálenosti v metrech místo ve stopách.")

# --- Controls -----------------------------------------------------------------------------------
TRANSLATIONS["DEM_SecControls"] = T(
    "操作", "조작", "控制", "Управление", "Steuerung",
    "Commandes", "Controles", "Comandi", "Sterowanie", "Ovládání")
TRANSLATIONS["DEM_HideKey"] = T(
    "非表示キー", "숨기기 키", "隐藏按键", "Клавиша скрытия", "Ausblendtaste",
    "Touche de masquage", "Tecla de ocultar", "Tasto per nascondere", "Klawisz ukrywania", "Klávesa skrytí")
TRANSLATIONS["DEM_HelpHideKey"] = T(
    "このキーでミニマップを即座に表示または非表示にします。0 で無効になります。",
    "이 키로 미니맵을 즉시 표시하거나 숨깁니다. 0이면 사용하지 않습니다.",
    "按此键可立即显示或隐藏小地图。设为 0 则禁用。",
    "Нажмите эту клавишу, чтобы сразу показать или скрыть миникарту. 0 отключает её.",
    "Mit dieser Taste blendest du die Minikarte sofort ein oder aus. 0 schaltet sie ab.",
    "Appuyez sur cette touche pour afficher ou masquer la minicarte immédiatement. 0 la désactive.",
    "Pulsa esta tecla para mostrar u ocultar el minimapa al instante. 0 la desactiva.",
    "Premi questo tasto per mostrare o nascondere subito la minimappa. 0 lo disattiva.",
    "Naciśnij ten klawisz, aby natychmiast pokazać lub ukryć minimapę. 0 go wyłącza.",
    "Touto klávesou okamžitě zobrazíš nebo skryješ minimapu. 0 ji vypne.")
TRANSLATIONS["DEM_NoHideKey"] = T(
    "非表示キーは設定されていません。", "숨기기 키가 설정되지 않았습니다.", "未设置隐藏按键。",
    "Клавиша скрытия не назначена.", "Keine Ausblendtaste festgelegt.",
    "Aucune touche de masquage définie.", "No hay tecla de ocultar definida.",
    "Nessun tasto per nascondere impostato.", "Nie ustawiono klawisza ukrywania.", "Není nastavena klávesa skrytí.")
TRANSLATIONS["DEM_ControllerButton"] = T(
    "コントローラー: タップで非表示、長押しでパン",
    "컨트롤러: 짧게 눌러 숨기기, 길게 눌러 이동",
    "手柄：轻按隐藏，长按平移",
    "Геймпад: нажатие - скрыть, удержание - панорама",
    "Controller: Tippen zum Ausblenden, Halten zum Verschieben",
    "Manette : appui bref pour masquer, maintien pour déplacer",
    "Mando: pulsación breve para ocultar, mantener para desplazar",
    "Controller: tocco per nascondere, tieni premuto per spostare",
    "Kontroler: krótkie naciśnięcie ukrywa, przytrzymanie przesuwa",
    "Ovladač: krátký stisk skryje, podržení posouvá")
TRANSLATIONS["DEM_HelpControllerButton"] = T(
    "既定ではオフです。オンにすると、下のコントローラーボタンをタップでミニマップの表示/非表示、長押し中は右スティックでマップをパンします。自分の配置で空いているボタンを選んでください。",
    "기본값은 꺼짐입니다. 켜면 아래 컨트롤러 버튼을 짧게 눌러 미니맵을 숨기거나 표시하고, 길게 누른 채 오른쪽 스틱으로 지도를 이동합니다. 자신의 배치에서 비어 있는 버튼을 고르세요.",
    "默认关闭。开启后，轻按下方的手柄按键可隐藏/显示小地图，按住时用右摇杆平移地图。请选择你布局中空闲的按键。",
    "По умолчанию выключено. Включено: короткое нажатие кнопки геймпада ниже скрывает или показывает миникарту, удержание позволяет двигать карту ПРАВЫМ стиком. Выберите кнопку, свободную в вашей раскладке.",
    "Standardmäßig aus. An: Ein Tippen auf die unten gewählte Controller-Taste blendet die Minikarte aus oder ein, Halten verschiebt die Karte mit dem RECHTEN Stick. Wähle eine Taste, die in deinem Layout frei ist.",
    "Désactivé par défaut. Activé : un appui bref sur le bouton de manette ci-dessous masque ou affiche la minicarte, le maintien permet de déplacer la carte avec le stick DROIT. Choisissez un bouton libre dans votre configuration.",
    "Desactivado por defecto. Activado: una pulsación breve del botón del mando indicado abajo oculta o muestra el minimapa; manteniéndolo pulsado se desplaza el mapa con el stick DERECHO. Elige un botón libre en tu configuración.",
    "Disattivato di default. Attivo: un tocco sul pulsante del controller indicato sotto nasconde o mostra la minimappa, tenendolo premuto si sposta la mappa con la levetta DESTRA. Scegli un pulsante libero nella tua configurazione.",
    "Domyślnie wyłączone. Włączone: krótkie naciśnięcie wskazanego niżej przycisku kontrolera ukrywa lub pokazuje minimapę, przytrzymanie przesuwa mapę PRAWĄ gałką. Wybierz przycisk wolny w twoim układzie.",
    "Ve výchozím stavu vypnuto. Zapnuto: krátký stisk níže zvoleného tlačítka ovladače skryje nebo zobrazí minimapu, podržení posouvá mapu PRAVOU páčkou. Vyber tlačítko, které je ve tvém rozložení volné.")
TRANSLATIONS["DEM_ControllerMask"] = T(
    "コントローラーボタン（XInput マスク）", "컨트롤러 버튼 (XInput 마스크)", "手柄按键（XInput 掩码）",
    "Кнопка геймпада (маска XInput)", "Controller-Taste (XInput-Maske)",
    "Bouton de manette (masque XInput)", "Botón del mando (máscara XInput)",
    "Pulsante del controller (maschera XInput)", "Przycisk kontrolera (maska XInput)", "Tlačítko ovladače (maska XInput)")
TRANSLATIONS["DEM_HelpControllerMask"] = T(
    "XInput ボタンマスク: 128 = R3（右スティック押し込み）、64 = L3、256 = LB、512 = RB、16 = Start、32 = Back、4096 A、8192 B、16384 X、32768 Y。",
    "XInput 버튼 마스크: 128 = R3(오른쪽 스틱 누르기), 64 = L3, 256 = LB, 512 = RB, 16 = Start, 32 = Back, 4096 A, 8192 B, 16384 X, 32768 Y.",
    "XInput 按键掩码：128 = R3（右摇杆按下），64 = L3，256 = LB，512 = RB，16 = Start，32 = Back，4096 A，8192 B，16384 X，32768 Y。",
    "Маски кнопок XInput: 128 = R3 (нажатие правого стика), 64 = L3, 256 = LB, 512 = RB, 16 = Start, 32 = Back, 4096 A, 8192 B, 16384 X, 32768 Y.",
    "XInput-Tastenmasken: 128 = R3 (rechter Stick gedrückt), 64 = L3, 256 = LB, 512 = RB, 16 = Start, 32 = Back, 4096 A, 8192 B, 16384 X, 32768 Y.",
    "Masques de boutons XInput : 128 = R3 (clic du stick droit), 64 = L3, 256 = LB, 512 = RB, 16 = Start, 32 = Back, 4096 A, 8192 B, 16384 X, 32768 Y.",
    "Máscaras de botones XInput: 128 = R3 (clic del stick derecho), 64 = L3, 256 = LB, 512 = RB, 16 = Start, 32 = Back, 4096 A, 8192 B, 16384 X, 32768 Y.",
    "Maschere dei pulsanti XInput: 128 = R3 (clic della levetta destra), 64 = L3, 256 = LB, 512 = RB, 16 = Start, 32 = Back, 4096 A, 8192 B, 16384 X, 32768 Y.",
    "Maski przycisków XInput: 128 = R3 (kliknięcie prawej gałki), 64 = L3, 256 = LB, 512 = RB, 16 = Start, 32 = Back, 4096 A, 8192 B, 16384 X, 32768 Y.",
    "Masky tlačítek XInput: 128 = R3 (stisk pravé páčky), 64 = L3, 256 = LB, 512 = RB, 16 = Start, 32 = Back, 4096 A, 8192 B, 16384 X, 32768 Y.")
TRANSLATIONS["DEM_ShowLocationName"] = T(
    "地名を表示", "장소 이름 표시", "显示地点名称", "Показывать название места", "Ortsnamen anzeigen",
    "Afficher le nom du lieu", "Mostrar el nombre del lugar", "Mostra il nome del luogo",
    "Pokaż nazwę lokacji", "Zobrazit název místa")
TRANSLATIONS["DEM_HelpShowLocationName"] = T(
    "マップの下に出る地名です。ゲームの言語で文字が欠けたり、文字がフレームからはみ出したりする場合はオフにしてください。この見出しはゲーム自身のインターフェイスフォントを使っており、この MOD では変更できません。",
    "지도 아래에 나오는 장소 이름입니다. 게임 언어에서 글자가 빠지거나 글자가 프레임을 벗어난다면 끄세요. 이 제목은 게임 자체의 인터페이스 글꼴을 사용하며, 이 모드로는 바꿀 수 없습니다.",
    "地图下方的地点名称。若你的游戏语言出现缺字或文字超出边框，请关闭此项 —— 该标题使用游戏自身的界面字体，本模组无法更改。",
    "Название места под картой. Отключите его, если в вашем языке игры пропадают символы или текст выходит за рамку: заголовок использует собственный интерфейсный шрифт игры, который этот мод изменить не может.",
    "Der Ortsname unter der Karte. Schalte ihn aus, wenn in deiner Spielsprache Zeichen fehlen oder der Text über den Rahmen hinausläuft - die Beschriftung nutzt die eigene Oberflächenschrift des Spiels, die diese Mod nicht ändern kann.",
    "Le nom du lieu sous la carte. Désactivez-le si la langue de votre jeu affiche des caractères manquants ou du texte qui déborde du cadre - le titre utilise la police d'interface du jeu, que ce mod ne peut pas changer.",
    "El nombre del lugar bajo el mapa. Desactívalo si el idioma de tu juego muestra caracteres ausentes o texto que se sale del marco: el título usa la fuente de interfaz propia del juego, que este mod no puede cambiar.",
    "Il nome del luogo sotto la mappa. Disattivalo se nella lingua del tuo gioco mancano caratteri o il testo esce dalla cornice: il titolo usa il font di interfaccia del gioco, che questa mod non può cambiare.",
    "Nazwa lokacji pod mapą. Wyłącz ją, jeśli w twoim języku gry brakuje znaków albo tekst wychodzi poza ramkę - tytuł używa własnej czcionki interfejsu gry, której ten mod nie może zmienić.",
    "Název místa pod mapou. Vypni jej, pokud se v jazyce tvé hry ztrácejí znaky nebo text přetéká přes rám - titulek používá vlastní písmo rozhraní hry, které tento mod nemůže změnit.")
TRANSLATIONS["DEM_RotateWithPlayer"] = T(
    "プレイヤーに合わせて回転", "플레이어에 맞춰 회전", "随玩家旋转", "Поворачивать вместе с игроком", "Mit dem Spieler drehen",
    "Pivoter avec le joueur", "Girar con el jugador", "Ruota con il giocatore", "Obracaj z graczem", "Otáčet s hráčem")
TRANSLATIONS["DEM_HelpRotateWithPlayer"] = T(
    "オン: ミニマップがプレイヤーの向いている方向に合わせて回ります。オフ: ローカルマップと同じく、常に北が上になります。",
    "켬: 미니맵이 플레이어가 바라보는 방향에 맞춰 회전합니다. 끔: 지역 지도처럼 항상 북쪽이 위입니다.",
    "开启：小地图随玩家视角朝向转动。关闭：如同本地地图，始终北方朝上。",
    "Включено: миникарта поворачивается туда, куда смотрит игрок. Выключено: север всегда сверху, как на локальной карте.",
    "An: Die Minikarte dreht sich in die Blickrichtung des Spielers. Aus: Norden ist immer oben, wie bei der lokalen Karte.",
    "Activé : la minicarte pivote vers l'endroit où regarde le joueur. Désactivé : le nord est toujours en haut, comme sur la carte locale.",
    "Activado: el minimapa gira hacia donde mira el jugador. Desactivado: el norte siempre arriba, como en el mapa local.",
    "Attivo: la minimappa ruota verso dove guarda il giocatore. Disattivo: il nord è sempre in alto, come nella mappa locale.",
    "Włączone: minimapa obraca się w stronę, w którą patrzy gracz. Wyłączone: północ zawsze u góry, jak na mapie lokalnej.",
    "Zapnuto: minimapa se otáčí tam, kam se hráč dívá. Vypnuto: sever je vždy nahoře, jako u místní mapy.")

# --- Debug --------------------------------------------------------------------------------------
TRANSLATIONS["DEM_SecDebug"] = T(
    "デバッグ", "디버그", "调试", "Отладка", "Debug",
    "Débogage", "Depuración", "Debug", "Debugowanie", "Ladění")
TRANSLATIONS["DEM_LogLevel"] = T(
    "ログレベル", "로그 수준", "日志级别", "Уровень журнала", "Protokollstufe",
    "Niveau de journal", "Nivel de registro", "Livello di log", "Poziom dziennika", "Úroveň protokolu")
TRANSLATIONS["DEM_HelpLogLevel"] = T(
    "プラグインがログに書き出す詳しさです。問題を追っているのでなければ Info のままにしてください。",
    "플러그인이 로그에 기록하는 상세 수준입니다. 문제를 추적하는 경우가 아니면 Info로 두세요.",
    "插件写入日志的详细程度。除非正在排查问题，否则请保持为 Info。",
    "Насколько подробно плагин пишет в свой журнал. Оставьте Info, если не ищете причину неполадки.",
    "Wie ausführlich das Plugin in sein Protokoll schreibt. Lass es auf Info, sofern du keinem Problem nachgehst.",
    "Le niveau de détail que le plugin écrit dans son journal. Laissez sur Info sauf si vous traquez un problème.",
    "Cuánto detalle escribe el plugin en su registro. Déjalo en Info salvo que estés investigando un problema.",
    "Quanto dettaglio il plugin scrive nel suo log. Lascialo su Info a meno che tu non stia cercando un problema.",
    "Jak szczegółowo wtyczka pisze do swojego dziennika. Zostaw na Info, chyba że tropisz problem.",
    "Jak podrobně plugin zapisuje do svého protokolu. Ponech na Info, pokud nehledáš příčinu problému.")

# --- the buttons row ----------------------------------------------------------------------------
TRANSLATIONS["DEM_Save"] = T(
    "保存", "저장", "保存", "Сохранить", "Speichern",
    "Enregistrer", "Guardar", "Salva", "Zapisz", "Uložit")
TRANSLATIONS["DEM_StatusSaving"] = T(
    "保存しています…", "저장 중...", "正在保存…", "Сохранение...", "Speichern ...",
    "Enregistrement...", "Guardando...", "Salvataggio...", "Zapisywanie...", "Ukládání...")
TRANSLATIONS["DEM_StatusSaved"] = T(
    "設定を保存しました。", "설정을 저장했습니다.", "设置已保存。", "Настройки сохранены.", "Einstellungen gespeichert.",
    "Paramètres enregistrés.", "Ajustes guardados.", "Impostazioni salvate.", "Ustawienia zapisane.", "Nastavení uloženo.")
TRANSLATIONS["DEM_StatusSaveFailed"] = T(
    "INI を書き込めませんでした。理由はログを確認してください。",
    "INI를 기록하지 못했습니다. 이유는 로그를 확인하세요.",
    "无法写入 INI。原因请查看日志。",
    "Не удалось записать INI. Причина в журнале.",
    "Die INI konnte nicht geschrieben werden. Der Grund steht im Protokoll.",
    "Impossible d'écrire le fichier INI. Voyez le journal pour la raison.",
    "No se pudo escribir el INI. Consulta el registro para saber por qué.",
    "Impossibile scrivere l'INI. Il motivo è nel log.",
    "Nie udało się zapisać pliku INI. Powód znajdziesz w dzienniku.",
    "Soubor INI se nepodařilo zapsat. Důvod najdeš v protokolu.")
TRANSLATIONS["DEM_HelpSave"] = T(
    "このページのすべての設定をプラグインの INI に書き込み、再起動後も残るようにします。",
    "이 페이지의 모든 설정을 플러그인의 INI에 기록해 재시작 후에도 남도록 합니다.",
    "将本页所有设置写入插件的 INI 文件，使其在重启后依然保留。",
    "Записывает все настройки этой страницы в INI плагина, чтобы они пережили перезапуск.",
    "Schreibt jede Einstellung dieser Seite in die INI des Plugins, damit sie einen Neustart übersteht.",
    "Écrit tous les réglages de cette page dans le fichier INI du plugin pour qu'ils survivent à un redémarrage.",
    "Escribe todos los ajustes de esta página en el INI del plugin para que sobrevivan a un reinicio.",
    "Scrive tutte le impostazioni di questa pagina nell'INI del plugin, così sopravvivono a un riavvio.",
    "Zapisuje wszystkie ustawienia z tej strony do pliku INI wtyczki, aby przetrwały ponowne uruchomienie.",
    "Zapíše všechna nastavení z této stránky do INI pluginu, aby přežila restart.")
TRANSLATIONS["DEM_ReloadFromIni"] = T(
    "INI から再読み込み", "INI에서 다시 불러오기", "从 INI 重新载入", "Перечитать INI", "Aus INI neu laden",
    "Recharger depuis l'INI", "Recargar desde el INI", "Ricarica dall'INI", "Wczytaj ponownie z INI", "Načíst znovu z INI")
TRANSLATIONS["DEM_StatusReloading"] = T(
    "再読み込みしています…", "다시 불러오는 중...", "正在重新载入…", "Перезагрузка...", "Neu laden ...",
    "Rechargement...", "Recargando...", "Ricaricamento...", "Wczytywanie ponowne...", "Načítání...")
TRANSLATIONS["DEM_StatusReloaded"] = T(
    "INI から設定を再読み込みしました。",
    "INI에서 설정을 다시 불러왔습니다.",
    "已从 INI 重新载入设置。",
    "Настройки перечитаны из INI.",
    "Einstellungen wurden aus der INI neu geladen.",
    "Paramètres rechargés depuis le fichier INI.",
    "Ajustes recargados desde el INI.",
    "Impostazioni ricaricate dall'INI.",
    "Ustawienia wczytano ponownie z pliku INI.",
    "Nastavení bylo znovu načteno z INI.")
TRANSLATIONS["DEM_StatusReloadFailed"] = T(
    "INI を読み込めませんでした。理由はログを確認してください。",
    "INI를 읽지 못했습니다. 이유는 로그를 확인하세요.",
    "无法读取 INI。原因请查看日志。",
    "Не удалось прочитать INI. Причина в журнале.",
    "Die INI konnte nicht gelesen werden. Der Grund steht im Protokoll.",
    "Impossible de lire le fichier INI. Voyez le journal pour la raison.",
    "No se pudo leer el INI. Consulta el registro para saber por qué.",
    "Impossibile leggere l'INI. Il motivo è nel log.",
    "Nie udało się odczytać pliku INI. Powód znajdziesz w dzienniku.",
    "Soubor INI se nepodařilo přečíst. Důvod najdeš v protokolu.")
TRANSLATIONS["DEM_HelpReload"] = T(
    "前回の保存以降にここで行った変更を破棄し、ディスクから INI を読み直します。ファイルを手で編集した内容もこれで反映されます。",
    "마지막 저장 이후 여기서 한 변경을 버리고 디스크에서 INI를 다시 읽습니다. 파일을 직접 편집한 내용도 반영됩니다.",
    "丢弃自上次保存以来在此处所做的更改，并从磁盘重新读取 INI。手动编辑该文件的内容也会一并生效。",
    "Отбрасывает все изменения, сделанные здесь после последнего сохранения, и заново читает INI с диска. Правки, внесённые в файл вручную, тоже подхватываются.",
    "Verwirft alle Änderungen, die seit dem letzten Speichern hier gemacht wurden, und liest die INI erneut von der Festplatte. Auch von Hand vorgenommene Änderungen an der Datei werden übernommen.",
    "Annule toutes les modifications faites ici depuis le dernier enregistrement et relit le fichier INI sur le disque. Les modifications faites à la main dans le fichier sont également reprises.",
    "Descarta cualquier cambio hecho aquí desde el último guardado y vuelve a leer el INI del disco. También recoge las ediciones hechas al archivo a mano.",
    "Scarta ogni modifica fatta qui dall'ultimo salvataggio e rilegge l'INI dal disco. Riprende anche le modifiche fatte al file a mano.",
    "Odrzuca wszystkie zmiany wprowadzone tutaj od ostatniego zapisu i ponownie czyta plik INI z dysku. Wychwytuje też zmiany wprowadzone do pliku ręcznie.",
    "Zahodí všechny změny provedené zde od posledního uložení a znovu načte INI z disku. Zachytí i úpravy provedené v souboru ručně.")
TRANSLATIONS["DEM_RestoreDefaults"] = T(
    "既定値に戻す", "기본값 복원", "恢复默认值", "Вернуть значения по умолчанию", "Standardwerte wiederherstellen",
    "Restaurer les valeurs par défaut", "Restaurar valores predeterminados", "Ripristina i valori predefiniti",
    "Przywróć domyślne", "Obnovit výchozí")
TRANSLATIONS["DEM_StatusDefaultsRestored"] = T(
    "既定値に戻しました。保存を押すと維持されます。",
    "기본값을 복원했습니다. 유지하려면 저장을 누르세요.",
    "已恢复默认值。按“保存”以保留。",
    "Значения по умолчанию восстановлены. Нажмите «Сохранить», чтобы оставить их.",
    "Standardwerte wiederhergestellt. Drücke Speichern, um sie zu behalten.",
    "Valeurs par défaut restaurées. Appuyez sur Enregistrer pour les conserver.",
    "Valores predeterminados restaurados. Pulsa Guardar para conservarlos.",
    "Valori predefiniti ripristinati. Premi Salva per mantenerli.",
    "Przywrócono wartości domyślne. Naciśnij Zapisz, aby je zachować.",
    "Výchozí hodnoty obnoveny. Stiskni Uložit, aby zůstaly.")
TRANSLATIONS["DEM_HelpRestoreDefaults"] = T(
    "すべての設定を新規インストール時の値に戻します。保存を押すまで何も書き込まれません。",
    "모든 설정을 새로 설치했을 때의 값으로 되돌립니다. 저장을 누르기 전까지는 아무것도 기록되지 않습니다.",
    "把所有设置恢复为全新安装时的值。在按下“保存”之前不会写入任何内容。",
    "Возвращает все настройки к значениям как при новой установке. Пока не нажата «Сохранить», ничего не записывается.",
    "Setzt jede Einstellung auf den Wert einer frischen Installation zurück. Es wird nichts geschrieben, bis du Speichern drückst.",
    "Remet chaque réglage à la valeur d'une installation neuve. Rien n'est écrit tant que vous n'appuyez pas sur Enregistrer.",
    "Devuelve todos los ajustes al valor de una instalación nueva. No se escribe nada hasta que pulses Guardar.",
    "Riporta ogni impostazione al valore di un'installazione nuova. Non viene scritto nulla finché non premi Salva.",
    "Przywraca wszystkim ustawieniom wartości ze świeżej instalacji. Nic nie zostanie zapisane, dopóki nie naciśniesz Zapisz.",
    "Vrátí všechna nastavení na hodnoty čerstvé instalace. Dokud nestiskneš Uložit, nic se nezapíše.")

# --- the page's intro line ----------------------------------------------------------------------
TRANSLATIONS["DEM_Intro"] = T(
    "変更は行った時点ですぐ反映されます。次回プレイ時も残すには保存を押してください。",
    "변경은 하는 즉시 적용됩니다. 다음에 플레이할 때도 유지하려면 저장을 누르세요.",
    "更改会在做出后立即生效。按“保存”可将其保留到下次游戏。",
    "Изменения применяются сразу же. Нажмите «Сохранить», чтобы сохранить их до следующей игры.",
    "Änderungen wirken sofort. Drücke Speichern, um sie für das nächste Spiel zu behalten.",
    "Les modifications s'appliquent dès que vous les faites. Appuyez sur Enregistrer pour les conserver pour la prochaine partie.",
    "Los cambios se aplican en cuanto los haces. Pulsa Guardar para conservarlos para la próxima vez que juegues.",
    "Le modifiche si applicano appena le fai. Premi Salva per mantenerle per la prossima partita.",
    "Zmiany działają natychmiast po ich wprowadzeniu. Naciśnij Zapisz, aby zachować je na następną grę.",
    "Změny se projeví hned, jak je uděláš. Stiskni Uložit, aby zůstaly i na příště.")

# --- the shape combo ----------------------------------------------------------------------------
TRANSLATIONS["DEM_Shape_Squared"] = T(
    "四角", "사각형", "方形", "Квадратная", "Quadratisch",
    "Carrée", "Cuadrado", "Quadrata", "Kwadratowa", "Čtvercová")
TRANSLATIONS["DEM_Shape_Round"] = T(
    "円形", "원형", "圆形", "Круглая", "Rund",
    "Ronde", "Redondo", "Rotonda", "Okrągła", "Kulatá")

# --- the corner combo ---------------------------------------------------------------------------
TRANSLATIONS["DEM_Corner_TopLeft"] = T(
    "左上", "좌측 상단", "左上", "Сверху слева", "Oben links",
    "En haut à gauche", "Arriba a la izquierda", "In alto a sinistra", "Lewy górny", "Vlevo nahoře")
TRANSLATIONS["DEM_Corner_TopRight"] = T(
    "右上", "우측 상단", "右上", "Сверху справа", "Oben rechts",
    "En haut à droite", "Arriba a la derecha", "In alto a destra", "Prawy górny", "Vpravo nahoře")
TRANSLATIONS["DEM_Corner_BottomLeft"] = T(
    "左下", "좌측 하단", "左下", "Снизу слева", "Unten links",
    "En bas à gauche", "Abajo a la izquierda", "In basso a sinistra", "Lewy dolny", "Vlevo dole")
TRANSLATIONS["DEM_Corner_BottomRight"] = T(
    "右下", "우측 하단", "右下", "Снизу справа", "Unten rechts",
    "En bas à droite", "Abajo a la derecha", "In basso a destra", "Prawy dolny", "Vpravo dole")

# --- the log levels -----------------------------------------------------------------------------
TRANSLATIONS["DEM_Log_Trace"] = T(
    "トレース", "추적", "追踪", "Трассировка", "Trace",
    "Trace", "Traza", "Traccia", "Śledzenie", "Trace")
TRANSLATIONS["DEM_Log_Debug"] = T(
    "デバッグ", "디버그", "调试", "Отладка", "Debug",
    "Débogage", "Depuración", "Debug", "Debugowanie", "Ladění")
TRANSLATIONS["DEM_Log_Info"] = T(
    "情報", "정보", "信息", "Информация", "Info",
    "Info", "Información", "Info", "Informacje", "Info")
TRANSLATIONS["DEM_Log_Warning"] = T(
    "警告", "경고", "警告", "Предупреждение", "Warnung",
    "Avertissement", "Advertencia", "Avviso", "Ostrzeżenie", "Varování")
TRANSLATIONS["DEM_Log_Error"] = T(
    "エラー", "오류", "错误", "Ошибка", "Fehler",
    "Erreur", "Error", "Errore", "Błąd", "Chyba")
TRANSLATIONS["DEM_Log_Critical"] = T(
    "重大", "심각", "严重", "Критическая", "Kritisch",
    "Critique", "Crítico", "Critico", "Krytyczny", "Kritické")
TRANSLATIONS["DEM_Log_Off"] = T(
    "オフ", "끄기", "关闭", "Выкл.", "Aus",
    "Désactivé", "Desactivado", "Disattivato", "Wyłączone", "Vypnuto")

# --- why a key cannot be bound ------------------------------------------------------------------
TRANSLATIONS["DEM_Res_Framework"] = T(
    "メニューフレームワークが使用しています", "메뉴 프레임워크가 사용 중입니다", "菜单框架正在使用它",
    "её использует меню-фреймворк", "das Menü-Framework verwendet sie",
    "le framework de menu l'utilise", "el framework de menús la usa",
    "il framework dei menu lo usa", "używa go framework menu", "používá ji framework nabídek")
TRANSLATIONS["DEM_Res_Tab"] = T(
    "Tab はトゥイーンメニューを開きます", "Tab은 트윈 메뉴를 엽니다", "Tab 用于打开快捷菜单",
    "Tab открывает меню Tween", "Tab öffnet das Tween-Menü",
    "Tab ouvre le menu Tween", "Tab abre el menú Tween",
    "Tab apre il menu Tween", "Tab otwiera menu Tween", "Tab otevírá nabídku Tween")
TRANSLATIONS["DEM_Res_Escape"] = T(
    "Escape はメニューを閉じます", "Escape는 메뉴를 닫습니다", "Escape 用于关闭菜单",
    "Escape закрывает меню", "Escape schließt Menüs",
    "Échap ferme les menus", "Escape cierra los menús",
    "Escape chiude i menu", "Escape zamyka menu", "Escape zavírá nabídky")
TRANSLATIONS["DEM_Res_MenuKey"] = T(
    "MOD 設定メニューを開くキーです", "모드 설정 메뉴를 여는 키입니다", "它用于打开模组配置菜单",
    "она открывает меню настроек модов", "sie öffnet das Mod-Konfigurationsmenü",
    "elle ouvre le menu de configuration des mods", "abre el menú de configuración de mods",
    "apre il menu di configurazione delle mod", "otwiera menu konfiguracji modów", "otevírá nabídku nastavení modů")
TRANSLATIONS["DEM_Res_Arrows"] = T(
    "矢印キーはメニュー操作に使われます", "화살표 키는 메뉴 이동에 쓰입니다", "方向键用于菜单导航",
    "стрелки управляют навигацией по меню", "Pfeiltasten steuern die Menünavigation",
    "les touches fléchées servent à naviguer dans les menus", "las flechas manejan la navegación por los menús",
    "i tasti freccia guidano la navigazione nei menu", "strzałki sterują nawigacją po menu", "šipky ovládají pohyb v nabídce")
TRANSLATIONS["DEM_Res_Enter"] = T(
    "Enter は選択中のコントロールを実行します", "Enter는 선택된 컨트롤을 실행합니다", "Enter 用于激活当前焦点控件",
    "Enter активирует выбранный элемент", "Enter aktiviert das fokussierte Element",
    "Entrée active l'élément sélectionné", "Intro activa el control enfocado",
    "Invio attiva il controllo selezionato", "Enter uruchamia zaznaczony element", "Enter aktivuje zaměřený prvek")
TRANSLATIONS["DEM_Res_Space"] = T(
    "Space は選択中のコントロールを実行します", "Space는 선택된 컨트롤을 실행합니다", "空格键用于激活当前焦点控件",
    "Пробел активирует выбранный элемент", "Leertaste aktiviert das fokussierte Element",
    "Espace active l'élément sélectionné", "Espacio activa el control enfocado",
    "Spazio attiva il controllo selezionato", "Spacja uruchamia zaznaczony element", "Mezerník aktivuje zaměřený prvek")


# ------------------------------------------------------------------------------------------------
# Writing the eleven files.
# ------------------------------------------------------------------------------------------------
def write_file(path, order, records):
    lines = []
    for key in order:
        lines.append("$" + key + "\t" + records[key].replace("\n", "\\n"))
    body = "\r\n".join(lines) + "\r\n"
    with io.open(path, "wb") as f:
        f.write(b"\xff\xfe")
        f.write(body.encode("utf-16-le"))


def main():
    order, english = read_keys()
    print("source/UI.cpp: {} keys".format(len(order)))

    missing = [k for k in order if k not in TRANSLATIONS]
    extra = [k for k in TRANSLATIONS if k not in english]
    if missing:
        raise SystemExit("no translations held for {} key(s): {}".format(len(missing), ", ".join(missing)))
    if extra:
        raise SystemExit("translations held for {} key(s) the source does not use: {}".format(len(extra), ", ".join(extra)))

    out_dir = os.path.join(REPO, "dist", "Interface", "Translations")
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    for lang in LANGS:
        if lang == "english":
            records = english
        else:
            records = {k: TRANSLATIONS[k][lang] for k in order}
        path = os.path.join(out_dir, "{}_{}.txt".format(STEM, lang))
        write_file(path, order, records)
        print("  {:9s} {:3d} keys -> {}".format(lang, len(order), os.path.basename(path)))


if __name__ == "__main__":
    main()
