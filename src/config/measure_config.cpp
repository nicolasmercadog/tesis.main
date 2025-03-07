
#include "measure_config.h"
#include "measure.h"

measure_config_t::measure_config_t() : BaseJsonConfig( MEASURE_JSON_CONFIG_FILE ) {}

bool measure_config_t::onSave(JsonDocument& doc) {

    doc["samplerate_corr"] = samplerate_corr;
    doc["network_frequency"] = network_frequency;

    for( int i = 0 ; i < MAX_GROUPS ; i++ ) {
        doc["group"][ i ]["name"] = measure_get_group_name( i );
        doc["group"][ i ]["active"] = measure_get_group_active( i );
    }

    for( int i = 0 ; i < VIRTUAL_CHANNELS ; i++ ) {
        char microcode[ VIRTUAL_CHANNELS * 3 ] = "";
        doc["channel"][ i ]["name"] = measure_get_channel_name( i );
        doc["channel"][ i ]["type"] = measure_get_channel_type( i );
        doc["channel"][ i ]["true_rms"] = measure_get_channel_true_rms( i );
        doc["channel"][ i ]["report_exp"] = measure_get_channel_report_exp( i );
        doc["channel"][ i ]["offset"] = measure_get_channel_offset( i );
        doc["channel"][ i ]["ratio"] = measure_get_channel_ratio( i );
        //doc["channel"][ i ]["escala_osc"] = measure_get_channel_escala_osc( i );
        doc["channel"][ i ]["phaseshift"] = measure_get_channel_phaseshift( i );
        doc["channel"][ i ]["group_id"] = measure_get_channel_group_id( i );
        doc["channel"][ i ]["mircocode"] = measure_get_channel_opcodeseq_str( i, sizeof( microcode ), microcode );
    }

    return true;
}

bool measure_config_t::onLoad(JsonDocument& doc) {

    samplerate_corr = doc["samplerate_corr"] | 0;
    network_frequency = doc["network_frequency"] | 50;

    for( int i = 0 ; i < MAX_GROUPS ; i++ ) {
        if( !doc["group"][ i ]["name"] )
            continue;
        measure_set_group_name( i, doc["group"][ i ]["name"] );
        measure_set_group_active( i, doc["group"][ i ]["active"] );
    }

    for( int i = 0 ; i < VIRTUAL_CHANNELS ; i++ ) {
        measure_set_channel_name( i, (char*) doc["channel"][ i ]["name"].as<String>().c_str() );
        measure_set_channel_type( i, doc["channel"][ i ]["type"] | AC_CURRENT );
        measure_set_channel_true_rms( i, doc["channel"][ i ]["true_rms"] | false );
        measure_set_channel_report_exp( i, doc["channel"][ i ]["report_exp"] | 0 );
        measure_set_channel_offset( i, doc["channel"][ i ]["offset"] | 0.0 );
        measure_set_channel_ratio( i, doc["channel"][ i ]["ratio"] | 1.0);
        //measure_set_channel_escala_osc( i, doc["channel"][ i ]["escala_osc"] | 1.0 );
        measure_set_channel_phaseshift( i, doc["channel"][ i ]["phaseshift"] | 0 );
        measure_set_channel_group_id( i, doc["channel"][ i ]["group_id"] | 0 );
        const char *opcodeseq_str = doc["channel"][ i ]["mircocode"].as<String>().c_str() ;
        measure_set_channel_opcodeseq_str( i, opcodeseq_str );        
    }

    return true;
}

bool measure_config_t::onDefault( void ) {

    return true;
}
