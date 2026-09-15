#include "vladbase.cpp"

int main() {
	vladbase::database db("base.db");

	char token[65] = "";
	char data[8128] = "";
	
    for (int i = 0; i<128; ++i) { // creating 128 records
		sprintf(token, "%d", i);
		int data_len = sprintf(data, "%d", i*123);
		db.add_record(token, data, data_len);
	}
	int status = db.read_record_with_offset("unexist record", data, 0, 32);
	if (status == 0) {
        printf("%s\n", data);
    }

	db.print_data_base();
    for (int i = 0; i<128; ++i) { // removing all this records
		sprintf(token, "%d", i);
		db.remove_record(token);
	}

    strcpy(token, "bible");
    if (!db.is_record_exitst(token)) {
		db.add_record(token, "The LORD is my shepherd; I shall not want. He maketh me to lie down in green pastures: he leadeth me beside the still waters. He restoreth my soul: he leadeth me in the paths of righteousness for his name's sake. Yea, though I walk through the valley of the shadow of death, I will fear no evil: for thou art with me; thy rod and thy staff they comfort me. Thou preparest a table before me in the presence of mine enemies: thou anointest my head with oil; my cup runneth over. Surely goodness and mercy shall follow me all the days of my life: and I will dwell in the house of the LORD for ever. The earth is the LORD'S, and the fulness thereof; the world, and they that dwell therein. For he hath founded it upon the seas, and established it upon the floods. Who shall ascend into the hill of the LORD? or who shall stand in his holy place? He that hath clean hands, and a pure heart; who hath not lifted up his soul unto vanity, nor sworn deceitfully. He shall receive the blessing from the LORD, and righteousness from the God of his salvation. This is the generation of them that seek him, that seek thy face, O Jacob. Selah. Lift up your heads, O ye gates; and be ye lift up, ye everlasting doors; and the King of glory shall come in. Who is this King of glory? The LORD strong and mighty, the LORD mighty in battle. Lift up your heads, O ye gates; even lift them up, ye everlasting doors; and the King of glory shall come in. Who is this King of glory? The LORD of hosts, he is the King of glory. Selah.", 1509);
	}
    db.write_to_record_end(token, " Some more text", 15);
	db.read_record(token, data, db.get_record_full_size(token));
    printf("%s\n", data);
}